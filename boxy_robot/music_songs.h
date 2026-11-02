#ifndef MUSIC_SONGS_H
#define MUSIC_SONGS_H

#ifndef BOXY_SONG
#define BOXY_SONG 0
#endif

#if BOXY_SONG < 0 || BOXY_SONG > 2
#error "BOXY_SONG must be 0 (custom), 1 (Boxy Riff), or 2 (Scale)"
#endif

#include "song.h"

#if BOXY_SONG == 0
const int *const selectedMelody = customsongMelody;
const int selectedMelodyNotes = customsongNotes;
const int selectedSongTempo = customsongTempo;
#elif BOXY_SONG == 1
const int boxyRiffMelody[] = {
  NOTE_C4, 8, NOTE_E4, 8, NOTE_G4, 8, NOTE_C5, 4,
  REST, 8, NOTE_G4, 8, NOTE_E4, 8, NOTE_C4, 4
};
const int *const selectedMelody = boxyRiffMelody;
const int selectedMelodyNotes = sizeof(boxyRiffMelody) / sizeof(boxyRiffMelody[0]) / 2;
const int selectedSongTempo = 120;
#elif BOXY_SONG == 2
const int scaleMelody[] = {
  NOTE_C4, 8, NOTE_D4, 8, NOTE_E4, 8, NOTE_F4, 8,
  NOTE_G4, 8, NOTE_A4, 8, NOTE_B4, 8, NOTE_C5, 4
};
const int *const selectedMelody = scaleMelody;
const int selectedMelodyNotes = sizeof(scaleMelody) / sizeof(scaleMelody[0]) / 2;
const int selectedSongTempo = 100;
#endif

#endif
