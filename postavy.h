#pragma once
#include <string>
#include <cstdlib>   // rand() - nespoliehat sa na to, ze ho dotiahne <string>

// STRUKTURA CHARAKTEROV V HRE
struct Bytost {
    std::string meno;
    int hp;         // aktuálne životy
    int maxHp;      // maximálne životy
    int ac;         // armor class (ako ťažko trafiť)
    int sila;       // Strength - modifikátor útoku a dmg
    int obratnost;  // Dexterity - napr. iniciatíva, AC neskôr
};

// ZBRAN AKO DATA, NIE AKO VETVENIE V KODE.
// Pridanie kuse alebo dyky je potom jeden riadok dat, nie nova vetva v zautoc.
struct Zbran {
    std::string nazov;
    int dosah;        // v dlazdiciach: mec 1, kratky luk 16 (80 stop)
    int kockyPocet;   // 1d8 -> kockyPocet 1, kockySteny 8
    int kockySteny;
    bool dialkova;    // dialkova sa riadi obratnostou a potrebuje vyhlad
};

// FUNKCIA NA HOD KOCKY (pocet = pocet kociek, steny = pocet stran, takze 2d4 bude pocet = 2, steny = 4)
inline int hod(int pocet, int steny) {
    int sucet = 0;
    for (int i = 0; i < pocet; i++) {
        sucet += rand() % steny + 1;
    }
    return sucet;
}

// FUNKCIA NA VYPOCET MODIFIERS AKO V DND
inline int modifikator(int atribut) {
    return (atribut - 10) / 2;
}

// FUNKCIA NA UTOK, VRACIA TEXT HLASKY
inline std::string zautoc(Bytost& utocnik, Bytost& obranca, const Zbran& zbran) {
    // dialkove zbrane sa v D&D riadia obratnostou, zblizka silou
    int atribut = utocnik.sila;
    if (zbran.dialkova) atribut = utocnik.obratnost;

    int hitRoll = hod(1, 20) + modifikator(atribut);
    if (hitRoll >= obranca.ac) {
        int dmg = hod(zbran.kockyPocet, zbran.kockySteny) + modifikator(atribut);
        obranca.hp -= dmg;
        if (obranca.hp < 0) obranca.hp = 0;
        return zbran.nazov + ": HIT! (-" + std::to_string(dmg) + " HP)";
    }
    return zbran.nazov + ": MISS!";
}

// FUNKCIA NA RESET STAVU BOJA (aby sa to nemuselo pisat na styroch miestach)
inline void resetBoja(bool& hracovTah, bool& akciaPouzita, float& cakanie, int& zostavaPohyb, int pohybNaKolo) {
    hracovTah = true;               // vždy začína hráč
    akciaPouzita = false;           // má voľnú akciu
    cakanie = 0.f;                  // žiadna pauza
    zostavaPohyb = pohybNaKolo;     // plný pohyb na kolo
}
