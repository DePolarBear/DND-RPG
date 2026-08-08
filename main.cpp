#include <SFML/Graphics.hpp>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>

#include "dialog.h"
#include "FastNoiseLite.h"

// PREPINANIE HERNEHO MODU MEDZI COMBAT A VOLNY POHYB
enum HernyStav { VOLNY_POHYB, COMBAT };

// FUNKCIA NA DETEKCIU VODY
bool jePrekazka(float x, float y, std::vector<std::vector<int>>& mapa, int VELKOST) {
    if (x < 0 || y < 0) {
        return false;   // mimo mapy vľavo/hore -> nie je prekážka
    }
    int stlpec = x / VELKOST;
    int riadok = y / VELKOST;
    if (riadok >= mapa.size() || stlpec >= mapa[0].size()) {
        return false;   // mimo mapy vpravo/dole
    }
    return mapa[riadok][stlpec] == 1;
}

//FUNKCIA NA DETEKCIU STROMOV
bool jeStrom(float x, float y, std::vector<std::vector<int>>& stromMapa, int VELKOST) {
    if (x < 0 || y < 0) return false;
    int stlpec = x / VELKOST;
    int riadok = y / VELKOST;
    if (riadok >= stromMapa.size() || stlpec >= stromMapa[0].size()) return false;
    return stromMapa[riadok][stlpec] == 1;
}

// FUNKCIA NA KOLIZIE
void pohyb(sf::RectangleShape& hrac, sf::Vector2f smer,
           std::vector<std::vector<int>>& mapa, int VELKOST,
           std::vector<std::vector<int>>& stromMapa) {
    sf::Vector2f stara = hrac.getPosition();
    hrac.move(smer);

    float stredX = hrac.getPosition().x + 20.f;
    float stredY = hrac.getPosition().y + 20.f;
    bool voda = jePrekazka(stredX, stredY, mapa, VELKOST);
    bool strom_kolizia = jeStrom(stredX, stredY, stromMapa, VELKOST);

    if (voda || strom_kolizia) {
        hrac.setPosition(stara);
    }
}

// FUNKCIA NA HOD KOCKY (pocet = pocet kociek, steny = pocet stran, takze 2d4 bude pocet = 2, steny = 4)
int hod(int pocet, int steny) {
    int sucet = 0;
    for (int i = 0; i < pocet; i++) {
        sucet += rand() % steny + 1;
    }
    return sucet;
}

// FUNKCIA NA VYPOCET MODIFIERS AKO V DND
int modifikator(int atribut) {
    return (atribut - 10) / 2;
}

// STRUKTURA CHARAKTEROV V HRE
struct Bytost {
    std::string meno;
    int hp;         // aktuálne životy
    int maxHp;      // maximálne životy
    int ac;         // armor class (ako ťažko trafiť)
    int sila;       // Strength - modifikátor útoku a dmg
    int obratnost;  // Dexterity - napr. iniciatíva, AC neskôr
};

std::string zautoc(Bytost& utocnik, Bytost& obranca) {
    int hitRoll = hod(1, 20) + modifikator(utocnik.sila);
    if (hitRoll >= obranca.ac) {
        int dmg = hod(1, 8) + modifikator(utocnik.sila);
        obranca.hp -= dmg;
        if (obranca.hp < 0) obranca.hp = 0;
        return utocnik.meno + " HIT! (-" + std::to_string(dmg) + " HP)";
    }
    return utocnik.meno + " MISS!";
}




///////////////////////////////////////////////////////////////////////////////////////////////////   MAIN     ///////////////////////////////////////////////////////////////////////////
int main() {
    // veci k mape
    srand(time(0));
    int VELKOST = 60;
    float rychlost = 300.f;
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
    bool questPrijaty = false;
    bool questSplneny = false;
    // combat
    HernyStav stav = VOLNY_POHYB;   // začíname voľným pohybom
    bool hracovTah = true;   // true = hráč, false = vlk
    std::string bojovaHlaska = "";

    sf::RenderWindow window(sf::VideoMode::getDesktopMode(), "RPG");
    sf::View view(sf::FloatRect({0.f, 0.f}, sf::Vector2f(window.getSize())));

    sf::Clock clock;

    sf::Font font;
    if (!font.openFromFile("C:/Windows/Fonts/arial.ttf")) {
        // font sa nenačítal - ošetríme
    }

    // VYTVORENIE POSTAVY HRACA
    sf::RectangleShape hrac(sf::Vector2f(40.f, 40.f));   // štvorec 40x40
    hrac.setFillColor(sf::Color(200, 100, 50));           // oranžová

    // VYTVORENIE OBJEKTU STROM
    sf::RectangleShape strom(sf::Vector2f(50.f, 50.f));
    strom.setFillColor(sf::Color(100, 50, 0));

    // VYTVORENIE NPC
    sf::RectangleShape npc(sf::Vector2f(40.f, 40.f));
    npc.setFillColor(sf::Color(220, 200, 40));   // žltá

    // VYTVORENIE NEPRIATELA VLK
    sf::RectangleShape vlkShape(sf::Vector2f(45.f, 45.f));
    vlkShape.setFillColor(sf::Color(150, 30, 30));   // tmavočervená

    //window.setVerticalSyncEnabled(true); 

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
            if (hodnota < -0.4f) {
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
    hrac.setPosition(sf::Vector2f(spawnC * VELKOST, spawnR * VELKOST));

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
    npc.setPosition(sf::Vector2f(npcC * VELKOST, npcR * VELKOST));

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
    vlkShape.setPosition(sf::Vector2f(vlkC * VELKOST, vlkR * VELKOST));

    // vytvorenie dlazdice na mape
    sf::RectangleShape dlazdica(sf::Vector2f(VELKOST, VELKOST));

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




    /////////////////////////////////////////////////////////////////////////////////////////////    HLAVNA LOOP WHILE      ////////////////////////////////////////////////////////////////
    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();   // čas od minulého frame v sekundách
        // na ktorej dlaždici je hráč
        int hrac_c = hrac.getPosition().x / VELKOST;
        int hrac_r = hrac.getPosition().y / VELKOST;
        // koľko dlaždíc okolo hráča kresliť (podľa veľkosti okna + rezerva)
        int dosah = 23;
        // pozicia hraca
        sf::Vector2f hpos = hrac.getPosition();
        sf::Vector2f npos = npc.getPosition();
        // vypocet vzdialenosti npc (je dost blizko na dialog?)
        bool blizko = (std::abs(hpos.x - npos.x) < 100 && std::abs(hpos.y - npos.y) < 100);
        // vypocet vzdialenost enemy (je dost blizko na combat?)
        if (stav == VOLNY_POHYB && aktivnyQuest.prijaty && !aktivnyQuest.splneny) {
            sf::Vector2f vpos = vlkShape.getPosition();
            if (std::abs(hpos.x - vpos.x) < 100 && std::abs(hpos.y - vpos.y) < 100) {
                stav = COMBAT;
            }
        }

        // POLLEVENT
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {  // Zatvaranie okna QUIT
                window.close();
            }
            if (const auto* resized = event->getIf<sf::Event::Resized>()) {  // resize view = nová veľkosť okna
                view.setSize(sf::Vector2f(resized->size));
                window.setView(view);
            }
            if (const auto* klik = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (klik->button == sf::Mouse::Button::Left) {
                    // pozícia kliku na obrazovke -> prepočet do sveta
                    sf::Vector2f svetovaKlik = window.mapPixelToCoords(klik->position);
                    // je klik na NPC a som blízko?
                    if (!dialogOtvoreny && blizko && npc.getGlobalBounds().contains(svetovaKlik)) {
                        dialogOtvoreny = true;
                        aktualnyUzol = 0;   // začni od úvodu
                    }
                    // klik na odpovede (keď dialóg otvorený)
                    else if (dialogOtvoreny) {
                        DialogUzol& uzol = dialog[aktualnyUzol];
                        sf::Vector2f npos = npc.getPosition();

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
            if (const auto* kl = event->getIf<sf::Event::KeyPressed>()) {
                if (kl->code == sf::Keyboard::Key::Escape) {
                    dialogOtvoreny = false;
                }
            }
            if (const auto* kl = event->getIf<sf::Event::KeyPressed>()) {
                if (kl->code == sf::Keyboard::Key::Escape) {
                    dialogOtvoreny = false;
                    stav = VOLNY_POHYB;   // dočasne: útek z boja
                }
            }
            if (const auto* kl = event->getIf<sf::Event::KeyPressed>()) {
                if (kl->code == sf::Keyboard::Key::Space) {
                    if (stav == COMBAT && hracovTah) {
                        bojovaHlaska = zautoc(hracB, vlk);   // ulož hlášku
                        hracovTah = false;       // ťah prejde na vlka
                    }
                }
            }
        }

        if (stav == VOLNY_POHYB) {
            // ZAKLDNY POHYB WASD + KOLIZIAA S VODOU A OBJEKTAMI (funkcia pohyb)
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
                pohyb(hrac, sf::Vector2f(0.f, -rychlost * dt), mapa, VELKOST, stromMapa);
            }
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
                pohyb(hrac, sf::Vector2f(-rychlost * dt, 0.f), mapa, VELKOST, stromMapa);
            }
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
                pohyb(hrac, sf::Vector2f(0.f, rychlost * dt), mapa, VELKOST, stromMapa);
            }
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
                pohyb(hrac, sf::Vector2f(rychlost * dt, 0.f), mapa, VELKOST, stromMapa);
            }
        }

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

        window.draw(hrac);
        window.draw(npc);
        if (aktivnyQuest.prijaty && !aktivnyQuest.splneny) window.draw(vlkShape);

        if (dialogOtvoreny) {
            DialogUzol& uzol = dialog[aktualnyUzol];
            sf::Vector2f npos = npc.getPosition();

            int pocetOdp = uzol.odpovede.size();
            float sirka = 580.f;
            float vyskaTextu = 120.f;                        // pevný priestor pre text (2-3 riadky)
            float vyska = vyskaTextu + pocetOdp * 50.f + 20.f;

            float bublinaX = npos.x - sirka / 2.f;
            float bublinaY = npos.y - vyska - 40.f;

            // bublina
            sf::RectangleShape bublina(sf::Vector2f(sirka, vyska));
            bublina.setFillColor(sf::Color(0, 0, 0, 200));
            bublina.setPosition(sf::Vector2f(bublinaX, bublinaY));
            window.draw(bublina);

            // text NPC (zalomenie priamo v texte cez \n v dialogy.h)
            sf::Text text(font);
            text.setString(uzol.text);
            text.setCharacterSize(28);
            text.setFillColor(sf::Color::White);
            text.setPosition(sf::Vector2f(bublinaX + 15.f, bublinaY + 10.f));
            window.draw(text);

            // odpovede
            for (int i = 0; i < pocetOdp; i++) {
                float y = bublinaY + vyskaTextu + i * 50.f;
                sf::RectangleShape tlacidlo(sf::Vector2f(sirka - 30.f, 40.f));
                tlacidlo.setFillColor(sf::Color(60, 60, 60, 220));
                tlacidlo.setPosition(sf::Vector2f(bublinaX + 15.f, y));
                window.draw(tlacidlo);

                sf::Text odpText(font);
                odpText.setString(uzol.odpovede[i].text);
                odpText.setCharacterSize(22);
                odpText.setPosition(sf::Vector2f(bublinaX + 25.f, y + 5.f));
                window.draw(odpText);
            }
        }

        window.setView(window.getDefaultView());
        sf::Vector2f oknoV = sf::Vector2f(window.getSize());

        sf::Text mojeHP(font);
        mojeHP.setString("HP: " + std::to_string(hracB.hp) + "/" + std::to_string(hracB.maxHp));
        mojeHP.setCharacterSize(30);
        mojeHP.setFillColor(sf::Color(220, 40, 40));   // červená

        sf::FloatRect tb = mojeHP.getLocalBounds();
        float boxSirka = tb.size.x + 40.f;
        float boxX = oknoV.x / 2.f - boxSirka / 2.f;   // vodorovne v strede
        float boxY = oknoV.y - 70.f;                    // dole

        sf::RectangleShape pozadie(sf::Vector2f(boxSirka, tb.size.y + 25.f));
        pozadie.setFillColor(sf::Color(60, 60, 60, 200));
        pozadie.setPosition(sf::Vector2f(boxX, boxY));
        window.draw(pozadie);

        mojeHP.setPosition(sf::Vector2f(boxX + 20.f, boxY + 5.f));
        window.draw(mojeHP);

        window.setView(view);

        if (aktivnyQuest.prijaty && !aktivnyQuest.splneny) {
            window.setView(window.getDefaultView());

            sf::Text questText(font);
            questText.setString("Quest: " + aktivnyQuest.ciel);
            questText.setCharacterSize(22);
            questText.setFillColor(sf::Color(255, 220, 100));

            // rozmer podľa textu
            sf::FloatRect tb = questText.getLocalBounds();
            float boxSirka = tb.size.x + 40.f;   // text + okraj
            float boxVyska = tb.size.y + 30.f;

            sf::RectangleShape questBox(sf::Vector2f(boxSirka, boxVyska));
            questBox.setFillColor(sf::Color(0, 0, 0, 180));
            questBox.setPosition(sf::Vector2f(20.f, 20.f));
            window.draw(questBox);

            questText.setPosition(sf::Vector2f(35.f, 30.f));
            window.draw(questText);

            window.setView(view);
        }

        if (stav == COMBAT) {
            sf::Vector2f vpos = vlkShape.getPosition();

            // pozadie baru (tmavé, plná šírka)
            float barSirka = 80.f;
            sf::RectangleShape barPozadie(sf::Vector2f(barSirka, 10.f));
            barPozadie.setFillColor(sf::Color(60, 60, 60));
            barPozadie.setPosition(sf::Vector2f(vpos.x - 20.f, vpos.y - 25.f));
            window.draw(barPozadie);

            // výplň baru (červená, podľa HP)
            float podiel = (float)vlk.hp / (float)vlk.maxHp;   // 0.0 až 1.0
            sf::RectangleShape barVypln(sf::Vector2f(barSirka * podiel, 10.f));
            barVypln.setFillColor(sf::Color(220, 40, 40));
            barVypln.setPosition(sf::Vector2f(vpos.x - 20.f, vpos.y - 25.f));
            window.draw(barVypln);

            // číslo HP nad barom
            sf::Text vlkHP(font);
            vlkHP.setString(std::to_string(vlk.hp) + "/" + std::to_string(vlk.maxHp));
            vlkHP.setCharacterSize(18);
            vlkHP.setFillColor(sf::Color::White);
            vlkHP.setPosition(sf::Vector2f(vpos.x - 15.f, vpos.y - 50.f));
            window.draw(vlkHP);
        }

        // vlkov ťah - keď nie je hráčov ťah
        if (stav == COMBAT && !hracovTah) {
            bojovaHlaska = zautoc(vlk, hracB);
            hracovTah = true;       // ťah späť na hráča
        }

        // kontrola konca boja
        if (stav == COMBAT) {
            if (vlk.hp <= 0) {
                stav = VOLNY_POHYB;              // koniec boja
                aktivnyQuest.splneny = true;      // quest splnený!
            }
            else if (hracB.hp <= 0) {
                stav = VOLNY_POHYB;              // hráč padol
                hracB.hp = hracB.maxHp;           // dočasne: obnov HP (respawn)
                // neskôr: prehra/game over
            }
        }

        if (stav == COMBAT && bojovaHlaska != "") {
            window.setView(window.getDefaultView());
            sf::Vector2f oknoV = sf::Vector2f(window.getSize());

            sf::Text hlaska(font);
            hlaska.setString(bojovaHlaska);
            hlaska.setCharacterSize(28);
            hlaska.setFillColor(sf::Color(255, 220, 100));
            hlaska.setPosition(sf::Vector2f(oknoV.x / 2.f - 150.f, 80.f));   // hore v strede
            window.draw(hlaska);

            window.setView(view);
        }

        window.display();
    }

    return 0;
}
