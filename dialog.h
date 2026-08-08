#pragma once
#include <vector>
#include <string>

struct Odpoved {
    std::string text;
    int kamVedie;      // index dalsieho uzla, -1 = zavri, -2 = daj quest
};

struct DialogUzol {
    std::string text;
    std::vector<Odpoved> odpovede;
};

struct Quest {
    std::string nazov;    // krátky názov
    std::string ciel;     // čo treba spraviť (text do okna)
    bool prijaty;         // prijal hráč quest?
    bool splneny;         // splnil ho?
};

inline std::vector<DialogUzol> vytvorDialog() {
    return {
        // 0 - uvod
        {
            "Konecne niekto! Uz som stratil nadej.",
            {
                {"Co sa deje?", 1},
                {"Kto si?", 2},
                {"Nemam cas.", -1}
            }
        },
        // 1 - co sa deje
        {
            "Nasu dedinu suzi bestia z lesa.\nKazdu noc nam berie dobytok.",
            {
                {"Aka bestia?", 3},
                {"A co ja s tym?", 4}
            }
        },
        // 2 - kto si
        {
            "Som Aldric, posledny strazca tejto dediny.\nOstatni utiekli.",
            {
                {"Preco utiekli?", 1},
                {"Mas pre mna ulohu?", 4}
            }
        },
        // 3 - aka bestia
        {
            "Velky tienovy vlk. Oci mu ziaria cervenou.\nNikto kto sa mu postavil to neprezil.",
            {
                {"A ty chces aby som to spravil ja?", 4}
            }
        },
        // 4 - ponuka questu
        {
            "Si jediny, kto sem za tie tyzdne prisiel.\nProsim najdi tu bestiu a znic ju.",
            {
                {"Beriem to. Kde ju najdem?", 5},
                {"Znie to nebezpecne...", 6},
                {"Nie, to nie je moja starost.", -1}
            }
        },
        // 5 - prijal, detaily
        {
            "Bludi na severe pri starych ruinach.\nBud opatrny a vrat sa s hlavou tej bestie!",
            {
                {"Idem na to.", -2}   // -2 = daj quest
            }
        },
        // 6 - vahanie
        {
            "Chapem tvoj strach. Ale ak to nespravis ty,\n dedina padne. Stedro ta odmenim!",
            {
                {"Dobre. Spravim to.", 5},
                {"Aj tak nie.", -1}
            }
        }
    };
}
