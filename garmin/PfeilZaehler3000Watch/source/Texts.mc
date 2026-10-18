// PfeilZaehler3000 (c) 2026 ckpt-gthb – free for personal, non-commercial use; PolyForm Strict 1.0.0 with permissions, see LICENSE
// Texts of the watch app in English, German and Polish.
// The language follows the watch (Automatic) or is chosen in the settings (hold UP).
// Texts with $1$ are filled in with Lang.format().

import Toybox.Lang;
import Toybox.System;

var gTexts = null;

function textTable() {
    if (gTexts == null) {
        gTexts = {
            //                 English                 Deutsch                    Polski
            :starting     => ["Starting...",           "Start ...",               "Start..."],
            :looking      => ["Looking for $1$...",    "Suche $1$ ...",           "Szukam: $1$..."],
            :inRange      => ["$1$ in range?",         "$1$ in Reichweite?",      "$1$ w zasięgu?"],
            :connecting   => ["Connecting...",         "Verbinde ...",            "Łączenie..."],
            :reconnecting => ["Reconnecting...",       "Verbinde neu ...",        "Ponowne łączenie..."],
            :updateFw     => ["Update pod firmware",   "Pod-Firmware erneuern",   "Zaktualizuj czujnik"],
            :connected    => ["$1$ connected",         "$1$ verbunden",           "$1$ połączony"],
            :low          => ["$1$ low $2$ V",         "$1$ schwach $2$ V",       "$1$ słaby $2$ V"],
            :endLine      => ["END",                   "PASSE",                   "SERIA"],
            :of           => ["of $1$",                "von $1$",                 "z $1$"],
            :end          => ["end",                   "Passe",                   "seria"],
            :session      => ["session",               "Training",                "trening"],
            :today        => ["today",                 "heute",                   "dziś"],
            :week         => ["week",                  "Woche",                   "tydzień"],
            :month        => ["month $1$",             "Monat $1$",               "miesiąc $1$"],
            :startRec     => ["START: record",         "START: Aufzeichnen",      "START: nagrywaj"],
            :sunsetIn     => ["sunset in $1$",         "Sonnenunterg. $1$",       "zachód za $1$"],
            :afterSunset  => ["after sunset",          "nach Sonnenunterg.",      "po zachodzie"],
            :noPosition   => ["sunset: no position",   "kein Standort",           "brak pozycji"],
            :noSunset     => ["sunset: n/a",           "nicht verfügbar",         "niedostępne"],
            :settings     => ["Settings",              "Einstellungen",           "Ustawienia"],
            :arrowsEnd    => ["Arrows per end",        "Pfeile pro Passe",        "Strzał w serii"],
            :newEndAfter  => ["New end after",         "Neue Passe nach",         "Nowa seria po"],
            :bigNumber    => ["Big number",            "Große Zahl",              "Duża liczba"],
            :timeSunset   => ["Time to sunset",        "Bis Sonnenuntergang",     "Czas do zachodu"],
            :mounting     => ["Mounting point",        "Montageort",              "Miejsce montażu"],
            :mount0       => ["Back of riser",         "Hinten am Mittelteil",    "Z tyłu majdanu"],
            :mount1       => ["Front of riser",        "Vorne am Mittelteil",     "Z przodu majdanu"],
            :mount2       => ["Side bushing",          "Seitlich",                "Z boku"],
            :language     => ["Language",              "Sprache",                 "Język"],
            :langAuto     => ["Automatic",             "Automatisch",             "Automatycznie"],
            :otherPod     => ["Use a different pod",   "Anderen Pod nutzen",      "Inny czujnik"],
            :otherPodQ    => ["Use a different pod?",  "Anderen Pod nutzen?",     "Użyć innego czujnika?"],
            :deleteLastQ  => ["Delete last arrow?",    "Letzten Pfeil löschen?",  "Usunąć ostatnią strzałę?"],
            :bigEnd       => ["This end",              "Diese Passe",             "Ta seria"],
            :bigSession   => ["Session",               "Training",                "Trening"],
            :bigToday     => ["Today",                 "Heute",                   "Dziś"],
            :current      => ["current",               "aktuell",                 "obecnie"],
            :scoreEnd     => ["END $1$",               "PASSE $1$",               "SERIA $1$"],
            :scoreEndOf   => ["END $1$ / $2$",         "PASSE $1$ / $2$",         "SERIA $1$ / $2$"],
            :clearShort   => ["Clear",                 "Neu",                     "Od nowa"],
            :saveShort    => ["OK",                    "OK",                      "OK"],
            :endSum       => ["end $1$",               "Passe $1$",               "seria $1$"],
            :totalAvg     => ["total $1$ · Ø $2$",     "gesamt $1$ · Ø $2$",      "razem $1$ · Ø $2$"],
            :ofMax        => ["of $1$",                "von $1$",                 "z $1$"],
            :vsAvg        => ["$1$ vs. Ø",             "$1$ zum Ø",               "$1$ wobec Ø"],
            :sessScore    => ["score $1$ · Ø $2$",     "Ergebnis $1$ · Ø $2$",    "wynik $1$ · Ø $2$"],
            :scoring      => ["Scoring",               "Wertung",                 "Punktacja"],
            :scoreAuto    => ["Opens by itself",       "Öffnet von selbst",       "Otwiera się sama"],
            :scoreManual  => ["By hand only",          "Nur von Hand",            "Tylko ręcznie"],
            :scoreOff     => ["Off",                   "Aus",                     "Wyłączona"],
            :design       => ["Score design",          "Wertungsansicht",         "Wygląd punktacji"],
            :dNumpad      => ["Numpad",                "Ziffernblock",            "Klawiatura"],
            :dDial        => ["Dial",                  "Ziffernring",             "Tarcza zegara"],
            :dPiMal       => ["PiMalDaumen",           "PiMalDaumen",             "PiMalDaumen"],
            :faceL        => ["Target face",           "Auflage",                 "Tarcza"],
            :face0        => ["Full face",             "Ganze Auflage",           "Cała tarcza"],
            :face1        => ["3-spot",                "Dreifach-Spot",           "Potrójna"],
            :scoreLast    => ["Score last end",        "Letzte Passe werten",     "Wynik ostatniej serii"],
            :noEndYet     => ["No end to score yet",   "Noch keine Passe",        "Brak serii"],
            :podLocked    => ["$1$ locked",            "$1$ gesperrt",            "$1$ zablokowany"],
            :version      => ["Version",               "Version",                 "Wersja"],
            :podFw        => ["Pod firmware $1$",      "Pod-Firmware $1$",        "Firmware czujnika $1$"]
        };
    }
    return gTexts;
}

// 0 English, 1 German, 2 Polish
function langIndex() {
    var pod = getPod();
    var pref = (pod != null) ? pod.langPref : 0;
    if (pref >= 1 && pref <= 3) {
        return pref - 1;
    }
    var sl = System.getDeviceSettings().systemLanguage;
    if (sl == System.LANGUAGE_DEU) {
        return 1;
    }
    if (sl == System.LANGUAGE_POL) {
        return 2;
    }
    return 0;
}

function tr(key) {
    var row = textTable()[key];
    if (row == null) {
        return "";
    }
    return (row as Array)[langIndex()] as String;
}

function trf(key, args) {
    return Lang.format(tr(key), args);
}
