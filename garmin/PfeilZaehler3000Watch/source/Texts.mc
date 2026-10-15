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
            :current      => ["current",               "aktuell",                 "obecnie"]
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
