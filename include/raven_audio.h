#pragma once
#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <driver/i2s.h>
#include "raven_config.h"

// Forward declarations
extern void seqCaw();
extern void seqAlert();
extern void seqWingFlap();
extern void seqIdle();
extern void seqSleep();

// ── I2S config ───────────────────────────────────────────────────────────────
#define I2S_NUM         I2S_NUM_0
#define I2S_BUFFER_SIZE 512

// ── Sound library ─────────────────────────────────────────────────────────────
struct SoundEntry {
    String file;
    String sequence;
    String label;
};

static std::vector<SoundEntry> soundLibrary;

void soundsLoad() {
    soundLibrary.clear();
    File f = SPIFFS.open("/sounds.json", "r");
    if (!f) { Serial.println("[audio] sounds.json not found"); return; }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) { Serial.printf("[audio] JSON error: %s\n", err.c_str()); return; }
    for (JsonObject obj : doc.as<JsonArray>()) {
        SoundEntry e;
        e.file     = obj["file"]     | "";
        e.sequence = obj["sequence"] | "none";
        e.label    = obj["label"]    | e.file;
        if (e.file.length()) soundLibrary.push_back(e);
    }
    Serial.printf("[audio] Loaded %d sounds\n", soundLibrary.size());
}

void soundsSave() {
    File f = SPIFFS.open("/sounds.json", "w");
    if (!f) { Serial.println("[audio] Failed to save sounds.json"); return; }
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (auto& e : soundLibrary) {
        JsonObject obj = arr.add<JsonObject>();
        obj["file"]     = e.file;
        obj["sequence"] = e.sequence;
        obj["label"]    = e.label;
    }
    serializeJson(doc, f);
    f.close();
}

void soundsUpsert(const String& file, const String& seq, const String& label) {
    for (auto& e : soundLibrary) {
        if (e.file == file) {
            e.sequence = seq;
            e.label    = label.length() ? label : e.label;
            soundsSave();
            return;
        }
    }
    soundLibrary.push_back({ file, seq, label.length() ? label : file });
    soundsSave();
}

void soundsRemove(const String& file) {
    soundLibrary.erase(
        std::remove_if(soundLibrary.begin(), soundLibrary.end(),
            [&](const SoundEntry& e){ return e.file == file; }),
        soundLibrary.end());
    soundsSave();
}

String soundsToJson() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (auto& e : soundLibrary) {
        JsonObject obj = arr.add<JsonObject>();
        obj["file"]     = e.file;
        obj["sequence"] = e.sequence;
        obj["label"]    = e.label;
    }
    String out;
    serializeJson(doc, out);
    return out;
}

// ── WAV header ────────────────────────────────────────────────────────────────
struct WavHeader {
    char     riff[4];
    uint32_t fileSize;
    char     wave[4];
    char     fmt[4];
    uint32_t fmtSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char     data[4];
    uint32_t dataSize;
};

// ── I2S initialisation ────────────────────────────────────────────────────────
void audioBegin() {
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = 44100,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 8,
        .dma_buf_len          = I2S_BUFFER_SIZE,
        .use_apll             = false,
        .tx_desc_auto_clear   = true,
    };
    i2s_pin_config_t pins = {
        .bck_io_num   = I2S_BCLK,
        .ws_io_num    = I2S_LRC,
        .data_out_num = I2S_DOUT,
        .data_in_num  = I2S_PIN_NO_CHANGE,
    };
    i2s_driver_install(I2S_NUM, &cfg, 0, nullptr);
    i2s_set_pin(I2S_NUM, &pins);
    i2s_zero_dma_buffer(I2S_NUM);
    Serial.println("[audio] I2S initialised");
}

// ── Playback task ─────────────────────────────────────────────────────────────
static TaskHandle_t audioTask   = nullptr;
static String       audioQueued = "";

void audioPlayTask(void*) {
    String path = "/" + audioQueued + ".wav";
    audioQueued  = "";

    File f = SPIFFS.open(path, "r");
    if (!f) {
        Serial.printf("[audio] Not found: %s\n", path.c_str());
        audioTask = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    WavHeader hdr;
    f.read((uint8_t*)&hdr, sizeof(hdr));

    if (strncmp(hdr.riff, "RIFF", 4) || strncmp(hdr.wave, "WAVE", 4)) {
        Serial.printf("[audio] Invalid WAV: %s\n", path.c_str());
        f.close();
        audioTask = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    Serial.printf("[audio] Playing %s — %uHz %ubit %uch\n",
        path.c_str(), hdr.sampleRate, hdr.bitsPerSample, hdr.numChannels);

    // Reconfigure I2S clock for this file
    i2s_set_clk(I2S_NUM,
        hdr.sampleRate,
        (i2s_bits_per_sample_t)hdr.bitsPerSample,
        I2S_CHANNEL_MONO);

    const bool is8bit = (hdr.bitsPerSample == 8);
    uint8_t  raw[I2S_BUFFER_SIZE];
    int16_t  out16[I2S_BUFFER_SIZE];
    size_t   written;

    while (f.available()) {
        size_t n = f.read(raw, sizeof(raw));
        if (!n) break;

        if (is8bit) {
            // Upsample 8-bit unsigned → 16-bit signed for I2S
            for (size_t i = 0; i < n; i++)
                out16[i] = ((int16_t)raw[i] - 128) << 8;
            i2s_write(I2S_NUM, out16, n * 2, &written, portMAX_DELAY);
        } else {
            i2s_write(I2S_NUM, raw, n, &written, portMAX_DELAY);
        }
        vTaskDelay(1);
    }

    i2s_zero_dma_buffer(I2S_NUM);
    f.close();
    Serial.printf("[audio] Done: %s\n", path.c_str());
    audioTask = nullptr;
    vTaskDelete(nullptr);
}

void audioPlay(const String& name) {
    if (audioTask) {
        vTaskDelete(audioTask);
        i2s_zero_dma_buffer(I2S_NUM);
        audioTask = nullptr;
    }
    audioQueued = name;
    xTaskCreate(audioPlayTask, "audio", 8192, nullptr, 2, &audioTask);
}

// ── Sequence launcher ─────────────────────────────────────────────────────────
void launchSequence(const String& seq) {
    if      (seq == "caw")      seqCaw();
    else if (seq == "alert")    seqAlert();
    else if (seq == "wingflap") seqWingFlap();
    else if (seq == "idle")     seqIdle();
    else if (seq == "sleep")    seqSleep();
}

// ── Play sound + bound sequence ───────────────────────────────────────────────
void soundPlay(const String& file) {
    audioPlay(file);
    for (auto& e : soundLibrary) {
        if (e.file == file) {
            launchSequence(e.sequence);
            return;
        }
    }
}
