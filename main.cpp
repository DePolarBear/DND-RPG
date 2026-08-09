#include <SFML/Graphics.hpp>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>
#include <algorithm>   // std::max pri hladani najlepsej dlazdice pre vlka

#include "dialog.h"
#include "ui.h"
#include "postavy.h"
#include "svet.h"
#include "FastNoiseLite.h"

// PREPINANIE HERNEHO MODU MEDZI COMBAT A VOLNY POHYB
enum HernyStav { VOLNY_POHYB, COMBAT };

///////////////////////////////////////////////////////////////////////   MAIN     ////////////////////////////////////////////////////////////////////

int main() {
    // veci k mape
    srand(time(0));
    int SIRKA_MAPY = 256;
    int VYSKA_MAPY = 256;
    // nájdi trávu blízko stredu mapy pre spawn
    int stredR = VYSKA_MAPY / 2;
    int stredC = SIRKA_MAPY / 2;
    int spawnR = stredR;
    int spawnC = stredC;
    // dialogy
    bool dialogOtvoreny = false;
    int aktualnyUzol = 0;   // začíname uzlom 0

    // combat - nastavenia (tu sa ladi rychlost boja)
    const float PAUZA = 1.f;          // sekundy medzi ťahmi
    const float DLZKA_HLASKY = 1.5f;  // ako dlho visí hláška nad hlavou
    // combat - stav
    HernyStav stav = VOLNY_POHYB;     // začíname voľným pohybom
    bool hracovTah = true;            // true = hráč, false = vlk
    bool akciaPouzita = false;        // minul už hráč akciu v tomto kole?
    float cakanie = 0.f;              // koľko sekúnd ešte čakáme pred ďalším ťahom
    float cooldownBoja = 0.f;         // po úteku sa boj chvíľu nespustí znova
    int zostavaPohyb = POHYB_V_KOLE;  // koľko dlaždíc ešte môže hráč prejsť v tomto kole
    AnimPohyb animHrac;               // kým beží, hráč práve kráča a nedá sa klikať
    AnimPohyb animVlk;
    bool vlkSaPohol = false;          // vlkov ťah má dve fázy: najprv príde, potom udrie
    int vybranaZbran = -1;            // -1 = žiadna, inak index do `zbrane` (režim cielenia)
    int cakajuciUtok = -1;            // zbraň, ktorou sa udrie AŽ po dôjdení k cieľu
    // MENU / PAUZA - naschval SAMOSTATNY bool a nie tretia hodnota v HernyStav:
    // pauza sa moze zapnut aj v boji a po zavreti menu sa musime vratit tam, kde sme boli.
    bool pauza = false;
    // combat - hlasky nad hlavami (kazdy ma svoju, aby sa neprepisovali)
    std::string hlaskaHrac = "";
    float casHlaskaHrac = 0.f;
    std::string hlaskaVlk = "";
    float casHlaskaVlk = 0.f;

    // OKNO BEZ RAMU CEZ CELU OBRAZOVKU (borderless fullscreen)
    // Style::None = ziadna titulkova lista ani ram, takze okno ma PRESNE velkost plochy
    // a nic z hry uz nevytrca dole mimo obrazovku. Alt+Tab funguje normalne.
    sf::RenderWindow window(sf::VideoMode::getDesktopMode(), "RPG", sf::Style::None);
    window.setPosition(sf::Vector2i(0, 0));

    sf::View view(sf::FloatRect({0.f, 0.f}, sf::Vector2f(window.getSize())));     // kamera vo svete
    // VLASTNY VIEW PRE UI namiesto window.getDefaultView() - ten sa pri zmene velkosti
    // okna NEAKTUALIZUJE a presne to sposobilo, ze tlacidla sa dali kliknut inde, nez boli.
    sf::View uiView(sf::FloatRect({0.f, 0.f}, sf::Vector2f(window.getSize())));

    sf::Clock clock;

    sf::Font font;
    if (!font.openFromFile("C:/Windows/Fonts/arial.ttf")) {
        // font sa nenačítal - ošetríme
    }

    // VYTVORENIE POSTAVY HRACA
    sf::RectangleShape hrac(sf::Vector2f(POSTAVA, POSTAVA));   // 48x48 v dlaždici 64
    hrac.setFillColor(sf::Color(200, 100, 50));           // oranžová

    // VYTVORENIE OBJEKTU STROM
    sf::RectangleShape strom(sf::Vector2f(VELKOST, VELKOST));   // strom vyplní celú dlaždicu
    strom.setFillColor(sf::Color(100, 50, 0));

    // VYTVORENIE NPC
    sf::RectangleShape npc(sf::Vector2f(POSTAVA, POSTAVA));
    npc.setFillColor(sf::Color(220, 200, 40));   // žltá

    // VYTVORENIE NEPRIATELA VLK
    sf::RectangleShape vlkShape(sf::Vector2f(POSTAVA, POSTAVA));   // rovnaká ako hráč, je to Medium tvor
    vlkShape.setFillColor(sf::Color(150, 30, 30));   // tmavočervená

    window.setVerticalSyncEnabled(true);

    // IMPORT KNIZNICE SO SUMOM (NOISE)
    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);   // typ šumu
    noise.SetFractalType(FastNoiseLite::FractalType_FBm);   // vrstvený šum
    noise.SetFractalOctaves(5);
    noise.SetFrequency(0.015f);                             // "priblíženie"
    noise.SetSeed(rand());                                  // random seed pri generovani mapy

    FastNoiseLite stromNoise;
    stromNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    stromNoise.SetFrequency(0.05f);   // vyššia frekvencia = menšie zhluky stromov
    stromNoise.SetSeed(rand());       // random seed pri generovani stromov

    // GENEROVANIE MAPY POMOCOU SUMU (NOISE)
    std::vector<std::vector<int>> mapa(VYSKA_MAPY, std::vector<int>(SIRKA_MAPY, 0));
    for (int r = 0; r < VYSKA_MAPY; r++) {
        for (int c = 0; c < SIRKA_MAPY; c++) {
            float hodnota = noise.GetNoise((float)c, (float)r);   // šum vráti -1 až 1
            if (hodnota < -0.6f) {
                mapa[r][c] = 1;        // voda (nízke)
            } else if (hodnota < 0.4f) {
                mapa[r][c] = 0;        // tráva (stredné)
            } else {
                mapa[r][c] = 2;        // kameň (vysoké)
            }
        }
    }

    // VYHLADZOVNAIE MAPY
    std::vector<std::vector<int>> vyhladena = mapa;   // kópia na zápis
    for (int r = 1; r < VYSKA_MAPY - 1; r++) {
        for (int c = 1; c < SIRKA_MAPY - 1; c++) {
            // spočítaj susedov každého typu
            int pocet[3] = {0, 0, 0};   // pocet[0]=tráva, [1]=voda, [2]=kameň
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    if (dr == 0 && dc == 0) continue;   // preskoč seba
                    int typ = mapa[r + dr][c + dc];
                    pocet[typ]++;
                }
            }
            // nájdi najčastejší typ v okolí
            int najcastejsi = 0;
            if (pocet[1] > pocet[najcastejsi]) najcastejsi = 1;
            if (pocet[2] > pocet[najcastejsi]) najcastejsi = 2;
            // ak je väčšina susedov (5+ z 8) iný typ, prispôsob sa
            if (pocet[najcastejsi] >= 5) {
                vyhladena[r][c] = najcastejsi;
            }
        }
    }
    mapa = vyhladena;   // nahraď pôvodnú vyhladenou

    // GENEROVANIE STROMOV
    std::vector<std::vector<int>> stromMapa(VYSKA_MAPY, std::vector<int>(SIRKA_MAPY, 0));
    for (int r = 0; r < VYSKA_MAPY; r++) {
        for (int c = 0; c < SIRKA_MAPY; c++) {
            if (mapa[r][c] == 0) {
                float s = stromNoise.GetNoise((float)c, (float)r);
                // strom len na každej druhej dlaždici (rozostup) A kde je šum vysoký
                if (s > 0.4f && r % 4 == 0 && c % 4 == 0 && rand() % 100 < 50) {
                    stromMapa[r][c] = 1;   // na tejto dlaždici je strom
                }
            }
        }
    }

    // HLADANIE SPAWNU PRE HRACA (niekde okolo centra mapy ale nemoze to byt voda)
    for (int dosah = 0; dosah < VYSKA_MAPY; dosah++) {
        bool naslo = false;
        for (int r = stredR - dosah; r <= stredR + dosah && !naslo; r++) {
            for (int c = stredC - dosah; c <= stredC + dosah && !naslo; c++) {
                if (r < 0 || c < 0 || r >= VYSKA_MAPY || c >= SIRKA_MAPY) continue;
                if (mapa[r][c] == 0) {   // tráva
                    spawnR = r;
                    spawnC = c;
                    naslo = true;
                }
            }
        }
        if (naslo) break;
    }
    // pripisanie pozicie hraca z hladania vyssie
    hrac.setPosition(naDlazdici(spawnR, spawnC));

    // NAHODNA POZICIA NPC NEDALEKO HRACA
    int npcR, npcC;
    while (true) {
        // hráčova dlaždica + náhodný posun -5 až +5 dlaždíc
        npcR = spawnR + (rand() % 11 - 5);
        npcC = spawnC + (rand() % 11 - 5);
        // musí byť v mape a na tráve
        if (npcR >= 0 && npcC >= 0 && npcR < VYSKA_MAPY && npcC < SIRKA_MAPY && mapa[npcR][npcC] == 0) {
            break;   // našli sme dobré miesto
        }
    }
    // pripisanie pozicie npc
    npc.setPosition(naDlazdici(npcR, npcC));

    // POZICIE QUEST NEPRIATELA NA SEVER OD HRACA
    int vlkR, vlkC;
    while (true) {
        vlkR = spawnR - (20 + rand() % 15);   // 20-34 dlaždíc na sever od hráča
        vlkC = spawnC + (rand() % 21 - 10);    // trochu do strán (-10 až +10)
        if (vlkR >= 0 && vlkC >= 0 && vlkR < VYSKA_MAPY && vlkC < SIRKA_MAPY && mapa[vlkR][vlkC] == 0) {
            break;
        }
    }
    // pripisanie pozicie pre nepriatela
    vlkShape.setPosition(naDlazdici(vlkR, vlkC));

    // vytvorenie dlazdice na mape + MRIEZKA
    sf::RectangleShape dlazdica(sf::Vector2f(VELKOST, VELKOST));
    // POZOR: hrubka ZAPORNA = obrys sa kresli DOVNUTRA. Kladna by presahovala
    // do susednej dlazdice a ciary by boli miestami dvojnasobne tmave.
    dlazdica.setOutlineThickness(-1.f);
    dlazdica.setOutlineColor(sf::Color(0, 0, 0, 45));

    // import dialogov z dialog.h
    std::vector<DialogUzol> dialog = vytvorDialog();

    Quest aktivnyQuest = {
        "Tienovy vlk",
        "Znic tienoveho vlka na severe",
        false,   // ešte neprijatý
        false    // ešte nesplnený
    };

    Bytost hracB = { "Hrdina", 30, 30, 14, 16, 12 };
    Bytost vlk   = { "Tienovy vlk", 20, 20, 13, 14, 16 };

    // ZBRANE HRACA - tlacidla v bare sa z tohto zoznamu generuju, takze pridanie
    // dalsej zbrane je jeden riadok tu a nikde inde sa nic nemeni.
    std::vector<Zbran> zbrane = {
        { "MEC", 1,  1, 8, false },    // 1d8, dosah 1 dlazdica
        { "LUK", 16, 1, 6, true  }     // 1d6, dostrel 16 dlazdic (80 stop), potrebuje vyhlad
    };
    Zbran hryzenie = { "Hryzenie", 1, 1, 8, false };   // zbran vlka




    /////////////////////////////////////////////////////////////////////////////////////////////    HLAVNA LOOP WHILE      ////////////////////////////////////////////////////////////////
    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();   // čas od minulého frame v sekundách

        // ANIMACIA CHODZE - musi byt PRED vypoctom dlazdic, nech su suradnice aktualne
        if (!pauza) {
            posunPoCeste(hrac, animHrac, dt, RYCHLOST_BOJ);
            posunPoCeste(vlkShape, animVlk, dt, RYCHLOST_BOJ);
        }
        // kym niekto kraca, hra caka - ziadne kliky, ziadny koniec kola, ziadny vlkov tah
        bool niektoIde = animHrac.bezi() || animVlk.bezi();
        // na ktorej dlaždici je hráč (počíta sa zo STREDU postavy, nie z rohu)
        sf::Vector2f hstred = stredTvaru(hrac);
        int hrac_c = hstred.x / VELKOST;
        int hrac_r = hstred.y / VELKOST;
        // koľko dlaždíc okolo hráča kresliť - odvodené z okna, nie napevno
        int dosah = (int)(uiView.getSize().x / 2.f / VELKOST) + 2;
        // stredy postáv (vzdialenosti sa merajú medzi stredmi, nie medzi rohmi)
        sf::Vector2f npos = npc.getPosition();   // ľavý horný roh NPC, dialógová bublina sa viaže naň
        sf::Vector2f nstred = stredTvaru(npc);
        // je NPC dost blizko na dialog? 2 dlazdice
        bool blizko = (std::abs(hstred.x - nstred.x) < 2.f * VELKOST &&
                       std::abs(hstred.y - nstred.y) < 2.f * VELKOST);

        // NA KTOREJ DLAZDICI STOJI VLK (pozor: vlkR/vlkC su pozicie zo spawnu, toto je aktualny stav)
        sf::Vector2f vlkStred = stredTvaru(vlkShape);
        int vlk_r = vlkStred.y / VELKOST;
        int vlk_c = vlkStred.x / VELKOST;
        // je vlk na susednej dlazdici? (melee dosah, aj diagonalne)
        bool vlkVDosahu = suSusedia(hrac_r, hrac_c, vlk_r, vlk_c);

        // KAM SA DA DOJST - BFS raz za frame, kym je hracov tah. Prazdne = nekresli sa nic.
        std::vector<std::vector<int>> dosahHraca;
        if (stav == COMBAT && hracovTah && !pauza && !niektoIde) {
            dosahHraca = dosahPohybu(hrac_r, hrac_c, zostavaPohyb, mapa, stromMapa, vlk_r, vlk_c);
        }

        // ROZMERY UI - RATANE RAZ, POUZIJU SA V KLIKU AJ V KRESLENI (ziadna duplicita!)
        // POZOR: rozmery UI VZDY z uiView, nie z window.getSize() - inak klik nesedi s tym, co vidis.
        sf::Vector2f oknoV = uiView.getSize();
        float barVyska = 110.f;
        float barY = oknoV.y - barVyska;
        float sirkaTlacidla = 190.f;
        float tlacidloY = barY + barVyska / 2.f - VYSKA_TLACIDLA / 2.f;   // zvisle v strede baru
        // tlacidla sa generuju z poctu zbrani + jedno na koniec kola, cele to vycentrovane
        int pocetTlacidiel = (int)zbrane.size() + 1;
        float celkovaSirka = pocetTlacidiel * sirkaTlacidla + (pocetTlacidiel - 1) * ODSTUP;
        float zaciatokX = oknoV.x / 2.f - celkovaSirka / 2.f;

        std::vector<sf::FloatRect> btnZbrane;
        for (int i = 0; i < (int)zbrane.size(); i++) {
            btnZbrane.push_back(sf::FloatRect({zaciatokX + i * (sirkaTlacidla + ODSTUP), tlacidloY},
                                              {sirkaTlacidla, VYSKA_TLACIDLA}));
        }
        sf::FloatRect btnKoniec({zaciatokX + zbrane.size() * (sirkaTlacidla + ODSTUP), tlacidloY},
                                {sirkaTlacidla, VYSKA_TLACIDLA});

        // ROZMERY MENU (v strede obrazovky)
        float menuSirka = 340.f;
        float menuVyska = 100.f + 2.f * VYSKA_TLACIDLA + 3.f * ODSTUP;
        sf::FloatRect menuBox({oknoV.x / 2.f - menuSirka / 2.f, oknoV.y / 2.f - menuVyska / 2.f},
                              {menuSirka, menuVyska});
        float menuTlacidloX = menuBox.position.x + ODSTUP;
        float menuTlacidloSirka = menuSirka - ODSTUP * 2.f;
        sf::FloatRect btnPokracovat({menuTlacidloX, menuBox.position.y + 80.f},
                                    {menuTlacidloSirka, VYSKA_TLACIDLA});
        sf::FloatRect btnKoniecHry({menuTlacidloX, menuBox.position.y + 80.f + VYSKA_TLACIDLA + ODSTUP},
                                   {menuTlacidloSirka, VYSKA_TLACIDLA});

        ///////////////////////////////   HERNA LOGIKA - CELA STOJI, KED JE PAUZA   ///////////////////////////////
        // POZOR: pollEvent a display sa NIKDY nepreskakuju, inak by Windows oznacil hru za "neodpoveda".
        // Zastavuje sa len to, co posuva hru dopredu: casovace, vstup do boja, pohyb a vlkov tah.
        if (!pauza) {
            // ODPOCITAVANIE VSETKYCH CASOVACOV (nikdy nie sleep, ten by zamrazil okno)
            if (cakanie > 0.f) cakanie -= dt;
            if (cooldownBoja > 0.f) cooldownBoja -= dt;
            if (casHlaskaHrac > 0.f) casHlaskaHrac -= dt;
            if (casHlaskaVlk > 0.f) casHlaskaVlk -= dt;

            // VSTUP DO BOJA (je dost blizko na combat?)
            if (stav == VOLNY_POHYB && aktivnyQuest.prijaty && !aktivnyQuest.splneny && cooldownBoja <= 0.f) {
                sf::Vector2f vstred = stredTvaru(vlkShape);
                if (std::abs(hstred.x - vstred.x) < 2.f * VELKOST &&
                    std::abs(hstred.y - vstred.y) < 2.f * VELKOST) {
                    stav = COMBAT;
                    resetBoja(hracovTah, akciaPouzita, cakanie, zostavaPohyb, POHYB_V_KOLE);
                    animHrac.zastav();
                    animVlk.zastav();
                    vlkSaPohol = false;
                    vybranaZbran = -1;
                    cakajuciUtok = -1;
                    // mimo boja sa chodi po pixeloch, v boji po dlazdiciach -> posad oboch na mriezku
                    naMriezku(hrac);
                    naMriezku(vlkShape);
                }
            }
        }

        // POLLEVENT
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {  // Zatvaranie okna QUIT
                window.close();
            }
            if (const auto* resized = event->getIf<sf::Event::Resized>()) {  // nová veľkosť okna
                view.setSize(sf::Vector2f(resized->size));                                    // kamera
                uiView = sf::View(sf::FloatRect({0.f, 0.f}, sf::Vector2f(resized->size)));    // UI
                window.setView(view);
            }
            if (const auto* klik = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (klik->button == sf::Mouse::Button::Left) {
                    // POZOR: dva rozne prepocty toho isteho kliku!
                    sf::Vector2f svetovaKlik = window.mapPixelToCoords(klik->position);   // veci vo svete (NPC) - kamera sa hýbe
                    // tlačidlá sú v UI, takže klik prepočítaj cez default view (nie surový pixel!)
                    sf::Vector2f mysUI = window.mapPixelToCoords(klik->position, uiView);

                    // ---------- KLIKANIE V MENU (ma prednost pred vsetkym ostatnym) ----------
                    if (pauza) {
                        if (btnPokracovat.contains(mysUI)) {
                            pauza = false;
                        }
                        else if (btnKoniecHry.contains(mysUI)) {
                            window.close();
                        }
                    }
                    // ---------- KLIKANIE V BOJI (tlacidla) ----------
                    else if (stav == COMBAT) {
                        if (hracovTah && cakanie <= 0.f && !niektoIde) {
                            // ---- 1) KLIK NA ZBRAN = zapnutie/vypnutie cielenia ----
                            bool klikNaZbran = false;
                            for (int i = 0; i < (int)btnZbrane.size(); i++) {
                                if (btnZbrane[i].contains(mysUI) && !akciaPouzita) {
                                    if (vybranaZbran == i) vybranaZbran = -1;   // druhý klik = zrušenie
                                    else vybranaZbran = i;
                                    klikNaZbran = true;
                                }
                            }

                            if (klikNaZbran) {
                                // nič viac, čaká sa na kliknutie na cieľ
                            }
                            else if (btnKoniec.contains(mysUI)) {
                                hracovTah = false;      // ťah prejde na vlka
                                akciaPouzita = false;   // pripravené na moje ďalšie kolo
                                vybranaZbran = -1;
                                cakanie = PAUZA;        // vlk začne až o sekundu
                            }
                            else if (svetovaKlik.x >= 0.f && svetovaKlik.y >= 0.f) {
                                int cielR = svetovaKlik.y / VELKOST;
                                int cielC = svetovaKlik.x / VELKOST;
                                bool klikNaVlka = (cielR == vlk_r && cielC == vlk_c);

                                // ---- 2) KLIK NA NEPRIATELA S VYBRANOU ZBRANOU = UTOK ----
                                if (klikNaVlka && vybranaZbran >= 0 && !akciaPouzita) {
                                    Zbran& z = zbrane[vybranaZbran];

                                    if (mozeZautocit(z, hrac_r, hrac_c, vlk_r, vlk_c, stromMapa)) {
                                        hlaskaHrac = zautoc(hracB, vlk, z);   // dosah je, útoč hneď
                                        casHlaskaHrac = DLZKA_HLASKY;
                                        akciaPouzita = true;
                                        vybranaZbran = -1;
                                    }
                                    else {
                                        // nemám dosah -> nájdi NAJLACNEJŠIU dlaždicu, z ktorej to už ide
                                        int najCena = -1;
                                        int najR = hrac_r;
                                        int najC = hrac_c;
                                        for (int ir = 0; ir < (int)dosahHraca.size(); ir++) {
                                            for (int ic = 0; ic < (int)dosahHraca[ir].size(); ic++) {
                                                int cena = dosahHraca[ir][ic];
                                                if (cena <= 0 || cena > zostavaPohyb) continue;
                                                int r = hrac_r - zostavaPohyb + ir;
                                                int c = hrac_c - zostavaPohyb + ic;
                                                if (!mozeZautocit(z, r, c, vlk_r, vlk_c, stromMapa)) continue;
                                                if (najCena < 0 || cena < najCena) {
                                                    najCena = cena;
                                                    najR = r;
                                                    najC = c;
                                                }
                                            }
                                        }
                                        if (najCena > 0) {
                                            // priblíž sa presne o toľko dlaždíc, koľko treba, a udri po dôjdení
                                            animHrac.body = cestaNaDlazdicu(dosahHraca, hrac_r, hrac_c,
                                                                            zostavaPohyb, najR, najC);
                                            animHrac.index = 0;
                                            zostavaPohyb -= najCena;
                                            cakajuciUtok = vybranaZbran;
                                            vybranaZbran = -1;
                                        }
                                        else {
                                            hlaskaHrac = "Nemam dosah!";   // ani po priblížení
                                            casHlaskaHrac = DLZKA_HLASKY;
                                        }
                                    }
                                }
                                // ---- 3) KLIK NA VOLNU DLAZDICU = OBYCAJNY POHYB ----
                                else {
                                    int cena = nakladNa(dosahHraca, hrac_r, hrac_c, zostavaPohyb, cielR, cielC);
                                    if (cena > 0 && cena <= zostavaPohyb) {
                                        animHrac.body = cestaNaDlazdicu(dosahHraca, hrac_r, hrac_c,
                                                                        zostavaPohyb, cielR, cielC);
                                        animHrac.index = 0;
                                        zostavaPohyb -= cena;   // pohyb sa mieňa, akcia ostáva
                                    }
                                }
                            }
                        }
                    }
                    // ---------- KLIKANIE MIMO BOJA (dialog s NPC) ----------
                    else {
                        // je klik na NPC a som blízko?
                        if (!dialogOtvoreny && blizko && npc.getGlobalBounds().contains(svetovaKlik)) {
                            dialogOtvoreny = true;
                            aktualnyUzol = 0;   // začni od úvodu
                        }
                        // klik na odpovede (keď dialóg otvorený)
                        else if (dialogOtvoreny) {
                            DialogUzol& uzol = dialog[aktualnyUzol];

                            int pocetOdp = uzol.odpovede.size();
                            float sirka = 580.f;
                            float vyskaTextu = 120.f;
                            float vyska = vyskaTextu + pocetOdp * 50.f + 20.f;
                            float bublinaX = npos.x - sirka / 2.f;
                            float bublinaY = npos.y - vyska - 40.f;

                            for (int i = 0; i < pocetOdp; i++) {
                                float y = bublinaY + vyskaTextu + i * 50.f;
                                sf::FloatRect box({bublinaX + 15.f, y}, {sirka - 30.f, 40.f});
                                if (box.contains(svetovaKlik)) {
                                    int kam = uzol.odpovede[i].kamVedie;
                                    if (kam == -1) dialogOtvoreny = false;
                                    else if (kam == -2) { dialogOtvoreny = false; aktivnyQuest.prijaty = true; }
                                    else aktualnyUzol = kam;
                                }
                            }
                        }
                    }
                }
            }
            if (const auto* kl = event->getIf<sf::Event::KeyPressed>()) {
                // ESCAPE = prepinac menu. Ked je otvoreny dialog, najprv zavrie ten.
                if (kl->code == sf::Keyboard::Key::Escape) {
                    if (dialogOtvoreny && !pauza) {
                        dialogOtvoreny = false;
                    }
                    else {
                        pauza = !pauza;   // zapni/vypni pauzu
                    }
                }
                // F = docasny utek z boja (predtym to robil Escape, ten uz otvara menu)
                if (kl->code == sf::Keyboard::Key::F && !pauza && stav == COMBAT) {
                    stav = VOLNY_POHYB;
                    resetBoja(hracovTah, akciaPouzita, cakanie, zostavaPohyb, POHYB_V_KOLE);
                    animHrac.zastav();
                    animVlk.zastav();
                    vlkSaPohol = false;
                    vybranaZbran = -1;
                    cakajuciUtok = -1;
                    cooldownBoja = 3.f;   // 3 sekundy sa boj nespustí znova, nech sa dá naozaj utiecť
                }
            }
        }

        // PREPOCITAJ, CI NIEKTO KRACA - klik v pollEvent mohol prave teraz rozbehnut chodzu
        // a hodnota z vrchu slucky je uz neplatna. Bez tohto by sa "utok po prichode"
        // vyhodnotil hned v tom istom frame, este na starej dlazdici ("Nemam dosah").
        niektoIde = animHrac.bezi() || animVlk.bezi();

        // OPAT LEN KED NIE JE PAUZA (pohyb, koniec boja, vlkov tah)
        if (!pauza) {
            if (stav == VOLNY_POHYB) {
                // ZAKLDNY POHYB WASD + KOLIZIAA S VODOU A OBJEKTAMI (funkcia pohyb)
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
                    pohyb(hrac, sf::Vector2f(0.f, -RYCHLOST * dt), mapa, stromMapa);
                }
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
                    pohyb(hrac, sf::Vector2f(-RYCHLOST * dt, 0.f), mapa, stromMapa);
                }
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
                    pohyb(hrac, sf::Vector2f(0.f, RYCHLOST * dt), mapa, stromMapa);
                }
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
                    pohyb(hrac, sf::Vector2f(RYCHLOST * dt, 0.f), mapa, stromMapa);
                }
            }

            // KONTROLA KONCA BOJA - MUSI BYT PRED VLKOVYM TAHOM (aby mrtvy vlk uz neudrel)
            if (stav == COMBAT) {
                if (vlk.hp <= 0) {
                    stav = VOLNY_POHYB;                              // koniec boja
                    aktivnyQuest.splneny = true;                     // quest splnený!
                    resetBoja(hracovTah, akciaPouzita, cakanie, zostavaPohyb, POHYB_V_KOLE);
                    animHrac.zastav();
                    animVlk.zastav();
                    vlkSaPohol = false;
                    vybranaZbran = -1;
                    cakajuciUtok = -1;
                }
                else if (hracB.hp <= 0) {
                    stav = VOLNY_POHYB;                              // hráč padol
                    hracB.hp = hracB.maxHp;                          // dočasne: obnov HP (respawn)
                    vlk.hp = vlk.maxHp;                              // vlk sa tiež vylieči, inak je to zadarmo
                    resetBoja(hracovTah, akciaPouzita, cakanie, zostavaPohyb, POHYB_V_KOLE);
                    animHrac.zastav();
                    animVlk.zastav();
                    vlkSaPohol = false;
                    vybranaZbran = -1;
                    cakajuciUtok = -1;
                    cooldownBoja = 3.f;                              // chvíľa na nadýchnutie
                    // neskôr: prehra/game over
                }
            }

            // UTOK PO PRICHODE - druha faza hracovho utoku, spusti sa az ked dokracal
            if (stav == COMBAT && hracovTah && cakajuciUtok >= 0 && !niektoIde) {
                Zbran& z = zbrane[cakajuciUtok];
                if (mozeZautocit(z, hrac_r, hrac_c, vlk_r, vlk_c, stromMapa)) {
                    hlaskaHrac = zautoc(hracB, vlk, z);
                    akciaPouzita = true;
                } else {
                    hlaskaHrac = "Nemam dosah!";   // cieľ sa medzitým zmenil
                }
                casHlaskaHrac = DLZKA_HLASKY;
                cakajuciUtok = -1;
            }

            // VLKOV TAH - az ked nie je hracov tah A ubehla pauza medzi tahmi
            if (stav == COMBAT && !hracovTah && cakanie <= 0.f && !niektoIde) {
                // FAZA 1: priblizenie. Vyberie z dosahu dlazdicu najblizsie k hracovi
                // (pri zhode vzdialenosti tu lacnejsiu) a rozbehne animaciu chodze.
                if (!vlkSaPohol) {
                    vlkSaPohol = true;
                    if (!vlkVDosahu) {
                        // kam vsade vlk dociahne v tomto kole
                        std::vector<std::vector<int>> dosahVlka =
                            dosahPohybu(vlk_r, vlk_c, POHYB_V_KOLE, mapa, stromMapa, hrac_r, hrac_c);

                        // A DRUHY BFS, tentokrat OD HRACA. Vzdusna ciara totiz klame:
                        // dlazdica za stromom vyzera blizko, ale obchadzka je dlha.
                        // Toto da skutocnu vzdialenost v krokoch a vlk uz nezabocuje do slepych ulic.
                        int radiusHraca = POHYB_V_KOLE * 2 + 2;
                        std::vector<std::vector<int>> odHraca =
                            dosahPohybu(hrac_r, hrac_c, radiusHraca, mapa, stromMapa, -1, -1);

                        int najR = vlk_r;
                        int najC = vlk_c;
                        int najVzd = nakladNa(odHraca, hrac_r, hrac_c, radiusHraca, vlk_r, vlk_c);
                        if (najVzd < 0) najVzd = vzdialenostDlazdic(vlk_r, vlk_c, hrac_r, hrac_c);
                        int najCena = 0;

                        for (int ir = 0; ir < (int)dosahVlka.size(); ir++) {
                            for (int ic = 0; ic < (int)dosahVlka[ir].size(); ic++) {
                                int cena = dosahVlka[ir][ic];
                                if (cena < 0) continue;   // sem sa nedostane
                                int r = vlk_r - POHYB_V_KOLE + ir;
                                int c = vlk_c - POHYB_V_KOLE + ic;

                                int vzd = nakladNa(odHraca, hrac_r, hrac_c, radiusHraca, r, c);
                                if (vzd < 0) continue;    // odtialto sa k hracovi vobec nedostane

                                if (vzd < najVzd || (vzd == najVzd && cena < najCena)) {
                                    najVzd = vzd;
                                    najCena = cena;
                                    najR = r;
                                    najC = c;
                                }
                            }
                        }
                        // rozbehni chodzu; utok pride az v dalsom prechode slucky, po dorazeni
                        animVlk.body = cestaNaDlazdicu(dosahVlka, vlk_r, vlk_c, POHYB_V_KOLE, najR, najC);
                        animVlk.index = 0;
                    }
                }
                // FAZA 2: akcia. Sem sa dostane az ked vlk dokracal (animacia dobehla).
                else {
                    if (vlkVDosahu) {
                        hlaskaVlk = zautoc(vlk, hracB, hryzenie);   // hláška ide nad vlka
                    } else {
                        hlaskaVlk = "Priblizuje sa...";
                    }
                    casHlaskaVlk = DLZKA_HLASKY;
                    hracovTah = true;                 // ťah späť na hráča
                    vlkSaPohol = false;               // pripravene na jeho dalsie kolo
                    zostavaPohyb = POHYB_V_KOLE;      // hráčovi sa obnoví pohyb na nové kolo
                    cakanie = PAUZA;                  // aj po vlkovi pauza, nech to stihneš prečítať
                }
            }
        }

        ///////////////////////////////////////////////////   KRESLENIE VO SVETE (kamera sa hybe)   ///////////////////////////////////////////////////
        view.setCenter(hrac.getPosition());
        window.setView(view);
        if (stav == COMBAT) {
            window.clear(sf::Color(60, 20, 20));   // tmavočervené pozadie v boji
        } else {
            window.clear(sf::Color(30, 30, 40));   // normálne
        }

        // VYKRESLOVANIE MAPY IBA V OKOLI HRACA (dosah = vzdialenost vykreslenia od hraca)
        for (int r = hrac_r - dosah; r <= hrac_r + dosah; r++) { // kreslenie mapy
            for (int c = hrac_c - dosah; c <= hrac_c + dosah; c++) {
                // ochrana: nekresli mimo mapy
                if (r < 0 || c < 0 || r >= VYSKA_MAPY || c >= SIRKA_MAPY) continue;
                    if (mapa[r][c] == 0) dlazdica.setFillColor(sf::Color(80, 160, 80));
                    if (mapa[r][c] == 1) dlazdica.setFillColor(sf::Color(50, 150, 255));
                    if (mapa[r][c] == 2) dlazdica.setFillColor(sf::Color(160, 160, 160));
                    dlazdica.setPosition(sf::Vector2f(float(c * VELKOST), float(r * VELKOST)));  // ako pri hracovi a strome
                    window.draw(dlazdica);
                    // VYKRESLOVANIE STROMOV IBA V OKOLI HRACA
                    if (stromMapa[r][c] == 1) {
                        strom.setPosition(sf::Vector2f(float(c * VELKOST), float(r * VELKOST)));
                        window.draw(strom);
                    }
            }
        }

        // DOSAH POHYBU - modre dlazdice, kam sa da v tomto kole dojst
        if (!dosahHraca.empty()) {
            sf::RectangleShape policko(sf::Vector2f(VELKOST, VELKOST));
            policko.setFillColor(UI_DOSAH);
            policko.setOutlineThickness(-1.f);
            policko.setOutlineColor(UI_DOSAH_OKRAJ);
            for (int ir = 0; ir < (int)dosahHraca.size(); ir++) {
                for (int ic = 0; ic < (int)dosahHraca[ir].size(); ic++) {
                    if (dosahHraca[ir][ic] <= 0) continue;   // -1 = nedosiahnutelne, 0 = tam uz stojim
                    int r = hrac_r - zostavaPohyb + ir;
                    int c = hrac_c - zostavaPohyb + ic;
                    policko.setPosition(sf::Vector2f(float(c * VELKOST), float(r * VELKOST)));
                    window.draw(policko);
                }
            }
        }

        // CIELENIE - ramik na vlkovi a pri dialkovej zbrani aj ciara vyhladu
        if (stav == COMBAT && vybranaZbran >= 0 && aktivnyQuest.prijaty && !aktivnyQuest.splneny) {
            Zbran& z = zbrane[vybranaZbran];
            bool daSa = mozeZautocit(z, hrac_r, hrac_c, vlk_r, vlk_c, stromMapa);

            // ak sa nedá odtiaľto, skús či existuje dlaždica v dosahu pohybu, z ktorej to ide
            if (!daSa) {
                for (int ir = 0; ir < (int)dosahHraca.size() && !daSa; ir++) {
                    for (int ic = 0; ic < (int)dosahHraca[ir].size() && !daSa; ic++) {
                        int cena = dosahHraca[ir][ic];
                        if (cena <= 0 || cena > zostavaPohyb) continue;
                        int r = hrac_r - zostavaPohyb + ir;
                        int c = hrac_c - zostavaPohyb + ic;
                        if (mozeZautocit(z, r, c, vlk_r, vlk_c, stromMapa)) daSa = true;
                    }
                }
            }

            sf::Color farbaCiela = UI_CIEL_NIE;
            if (daSa) farbaCiela = UI_CIEL_OK;

            sf::RectangleShape ramik(sf::Vector2f(VELKOST, VELKOST));
            ramik.setPosition(sf::Vector2f(float(vlk_c * VELKOST), float(vlk_r * VELKOST)));
            ramik.setFillColor(sf::Color::Transparent);
            ramik.setOutlineThickness(-3.f);
            ramik.setOutlineColor(farbaCiela);
            window.draw(ramik);

            // ciara vyhladu - hned vidno, ci strom zavadzia
            if (z.dialkova) {
                sf::VertexArray ciara(sf::PrimitiveType::Lines, 2);
                ciara[0].position = stredTvaru(hrac);
                ciara[1].position = stredTvaru(vlkShape);
                ciara[0].color = farbaCiela;
                ciara[1].color = farbaCiela;
                window.draw(ciara);
            }
        }

        window.draw(hrac);
        window.draw(npc);
        if (aktivnyQuest.prijaty && !aktivnyQuest.splneny) window.draw(vlkShape);

        // HP BAR NAD VLKOM (vo svete, lebo sa viaze na jeho poziciu)
        if (stav == COMBAT) {
            sf::Vector2f vpos = vlkShape.getPosition();
            float stredVlka = stredTvaru(vlkShape).x;

            sf::FloatRect vlkBar({stredVlka - 40.f, vpos.y - 28.f}, {80.f, 10.f});
            kresliBar(window, vlkBar, (float)vlk.hp / (float)vlk.maxHp, UI_CERVENA, UI_TMAVOCERVENA);

            kresliTextVStrede(window, font, std::to_string(vlk.hp) + " / " + std::to_string(vlk.maxHp),
                              sf::Vector2f(stredVlka, vpos.y - 45.f), PISMO_MALE, UI_TEXT, true);
        }

        // HLASKY NAD HLAVAMI (kazdy ma svoju, samy zmiznu ked dobehne casovac)
        // vlkova ide vyssie (75), aby sa nebila s jeho HP barom a cislom
        if (casHlaskaHrac > 0.f) {
            kresliHlasku(window, font, hlaskaHrac, stredTvaru(hrac), UI_ZLATA, 65.f);
        }
        if (casHlaskaVlk > 0.f && aktivnyQuest.prijaty && !aktivnyQuest.splneny) {
            // 100 = nad HP barom aj nad cislom (obe su nad vlkom), meria sa od STREDU postavy
            kresliHlasku(window, font, hlaskaVlk, stredTvaru(vlkShape), UI_NEPRIATEL, 100.f);
        }

        // DIALOGOVA BUBLINA (tiez vo svete, lebo visi nad NPC)
        if (dialogOtvoreny) {
            DialogUzol& uzol = dialog[aktualnyUzol];

            int pocetOdp = uzol.odpovede.size();
            float sirka = 580.f;
            float vyskaTextu = 120.f;                        // pevný priestor pre text (2-3 riadky)
            float vyska = vyskaTextu + pocetOdp * 50.f + 20.f;

            float bublinaX = npos.x - sirka / 2.f;
            float bublinaY = npos.y - vyska - 40.f;

            // bublina
            kresliPanel(window, sf::FloatRect({bublinaX, bublinaY}, {sirka, vyska}), UI_TMAVA, UI_OKRAJ);

            // text NPC (viacriadkovy, zalomenie cez \n v dialog.h, preto sa NEcentruje)
            sf::Text text(font);
            text.setString(uzol.text);
            text.setCharacterSize(PISMO_VELKE);
            text.setFillColor(UI_TEXT);
            text.setPosition(sf::Vector2f(bublinaX + ODSTUP, bublinaY + ODSTUP / 2.f));
            window.draw(text);

            // odpovede
            for (int i = 0; i < pocetOdp; i++) {
                float y = bublinaY + vyskaTextu + i * 50.f;
                sf::FloatRect odpBox({bublinaX + 15.f, y}, {sirka - 30.f, 40.f});
                kresliPanel(window, odpBox, UI_PANEL_SVETLY, UI_OKRAJ);
                kresliTextVlavo(window, font, uzol.odpovede[i].text,
                                sf::Vector2f(odpBox.position.x + ODSTUP, odpBox.position.y + odpBox.size.y / 2.f),
                                PISMO_NORMAL, UI_TEXT);
            }
        }

        ///////////////////////////////////////////////////   KRESLENIE UI (pevne na obrazovke)   ///////////////////////////////////////////////////
        window.setView(uiView);

        // QUESTOVE OKNO VLAVO HORE (rozmer podla textu)
        if (aktivnyQuest.prijaty && !aktivnyQuest.splneny) {
            std::string questNapis = "Quest: " + aktivnyQuest.ciel;

            // meranie textu, aby okno sedelo na jeho sirku
            sf::Text merac(font);
            merac.setString(questNapis);
            merac.setCharacterSize(PISMO_NORMAL);
            sf::FloatRect tbQ = merac.getLocalBounds();

            sf::FloatRect questBox({ODSTUP, ODSTUP},
                                   {tbQ.size.x + ODSTUP * 2.f, tbQ.size.y + ODSTUP * 1.5f});
            kresliPanel(window, questBox, UI_TMAVA, UI_OKRAJ);
            kresliTextVStrede(window, font, questNapis,
                              questBox.position + questBox.size / 2.f, PISMO_NORMAL, UI_ZLATA);
        }

        ///////////////////////////////   SPODNY BAR (vzdy viditelny, akcie pribudnu v boji)   ///////////////////////////////
        // okraj panelu naschval presahuje mimo obrazovky, nech je zlata linka vidiet len HORE
        kresliPanel(window, sf::FloatRect({-4.f, barY}, {oknoV.x + 8.f, barVyska + 8.f}), UI_TMAVA, UI_OKRAJ);

        // VLAVO: meno a HP hraca
        sf::FloatRect hpBox({ODSTUP * 2.f, barY + barVyska / 2.f}, {260.f, 26.f});
        kresliTextVlavo(window, font, hracB.meno,
                        sf::Vector2f(ODSTUP * 2.f, barY + ODSTUP * 2.f), PISMO_NORMAL, UI_TEXT);
        kresliBar(window, hpBox, (float)hracB.hp / (float)hracB.maxHp, UI_CERVENA, UI_TMAVOCERVENA);
        kresliTextVStrede(window, font, std::to_string(hracB.hp) + " / " + std::to_string(hracB.maxHp),
                          hpBox.position + hpBox.size / 2.f, PISMO_MALE, UI_TEXT);

        // STRED A VPRAVO: akcie a indikator tahu - LEN V BOJI
        if (stav == COMBAT) {
            std::string tahText = "TAH NEPRIATELA";
            sf::Color tahFarba = UI_NEPRIATEL;
            if (hracovTah) {
                tahText = "TVOJ TAH";
                tahFarba = UI_ZLATA;
            }
            kresliTextVStrede(window, font, tahText,
                              sf::Vector2f(oknoV.x - 150.f, barY + 38.f), PISMO_NORMAL, tahFarba);
            // kolko dlazdic este mozem prejst v tomto kole
            kresliTextVStrede(window, font,
                              "POHYB: " + std::to_string(zostavaPohyb) + " / " + std::to_string(POHYB_V_KOLE),
                              sf::Vector2f(oknoV.x - 150.f, barY + 74.f), PISMO_MALE, UI_TEXT);

            // tlacidla kreslime len ked je hrac naozaj na rade a nikto neprechadza
            if (hracovTah && cakanie <= 0.f && !niektoIde) {
                for (int i = 0; i < (int)zbrane.size(); i++) {
                    sf::Color farba = UI_CERVENA;
                    if (vybranaZbran == i) farba = UI_VYBRANE;   // vybraná zbraň svieti
                    kresliTlacidlo(window, font, btnZbrane[i], zbrane[i].nazov, farba, !akciaPouzita);
                }
                kresliTlacidlo(window, font, btnKoniec, "KONIEC KOLA", UI_MODRA, true);
            }
            // navod, co robit dalej
            if (vybranaZbran >= 0) {
                kresliTextVStrede(window, font, "Klikni na nepriatela",
                                  sf::Vector2f(oknoV.x / 2.f, barY - 22.f), PISMO_MALE, UI_ZLATA);
            }
        }

        ///////////////////////////////   MENU / PAUZA (kresli sa uplne nakoniec, teda NAD vsetkym)   ///////////////////////////////
        if (pauza) {
            // zavoj cez celu obrazovku, nech je jasne, ze hra stoji (okraj priehladny)
            kresliPanel(window, sf::FloatRect({0.f, 0.f}, oknoV), UI_ZAVOJ, sf::Color::Transparent);

            kresliPanel(window, menuBox, UI_TMAVA, UI_OKRAJ);
            kresliTextVStrede(window, font, "PAUZA",
                              sf::Vector2f(menuBox.position.x + menuBox.size.x / 2.f, menuBox.position.y + 45.f),
                              PISMO_VELKE, UI_ZLATA);

            kresliTlacidlo(window, font, btnPokracovat, "POKRACOVAT", UI_MODRA, true);
            kresliTlacidlo(window, font, btnKoniecHry, "KONIEC HRY", UI_CERVENA, true);
        }

        window.setView(view);   // vratenie kamery, nech dalsi frame zacina spravne
        window.display();
    }

    return 0;
}
