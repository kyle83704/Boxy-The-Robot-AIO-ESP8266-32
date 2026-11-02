#include <Arduino.h>

#include "hardware_config.h"
#include "music_player.h"
#include "music_songs.h"

namespace {
int songIndex = 0;
unsigned long lastNoteTime = 0;
int wholeNote = 0;
}

void resetMusic() {
  songIndex = 0;
  lastNoteTime = 0;
  wholeNote = 0;
}

void handleMusic(bool shouldPlay, bool hornIsOn) {
  if (!shouldPlay || hornIsOn || !BOXY_ENABLE_BUZZER) {
    return;
  }

  if (wholeNote == 0) {
    wholeNote = (60000 * 4) / selectedSongTempo;
    songIndex = 0;
    lastNoteTime = millis();
  }

  if (millis() < lastNoteTime) {
    return;
  }

  if (songIndex >= selectedMelodyNotes * 2) {
    songIndex = 0;
  }

  const int divider = selectedMelody[songIndex + 1];
  const int noteDuration = divider > 0
      ? wholeNote / divider
      : (wholeNote / abs(divider)) * 3 / 2;
  const int note = selectedMelody[songIndex];
  if (note == REST) {
    boxyNoTone();
  } else {
    boxyTone(note, noteDuration * 9 / 10);
  }

  lastNoteTime = millis() + noteDuration;
  songIndex += 2;
}
