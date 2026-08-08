#include <SFML/Graphics.hpp>
#include <vector>
#include "FastNoiseLite.h"

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

void pohyb(sf::RectangleShape& hrac, sf::Vector2f smer, std::vector<std::vector<int>>& mapa, int VELKOST) {
    sf::Vector2f stara = hrac.getPosition();
    hrac.move(smer);
    float stredX = hrac.getPosition().x + 20.f;
    float stredY = hrac.getPosition().y + 20.f;
    if (jePrekazka(stredX, stredY, mapa, VELKOST)) {
        hrac.setPosition(stara);
    }
}

int main() {
    int VELKOST = 60;
    float rychlost = 300.f;
    int SIRKA_MAPY = 256;
    int VYSKA_MAPY = 256;

    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);   // typ šumu
    noise.SetFractalType(FastNoiseLite::FractalType_FBm);   // vrstvený šum
    noise.SetFractalOctaves(5);
    noise.SetFrequency(0.015f);                             // "priblíženie"

    FastNoiseLite stromNoise;
    stromNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    stromNoise.SetFrequency(0.05f);   // vyššia frekvencia = menšie zhluky stromov
    stromNoise.SetSeed(1337);        // iný seed než terén, nech je to iný vzor

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

    // vyhladenie - po generovaní šumom
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

    std::vector<sf::Vector2f> stromy;   // zoznam pozícií stromov
    for (int r = 0; r < VYSKA_MAPY; r++) {
        for (int c = 0; c < SIRKA_MAPY; c++) {
            if (mapa[r][c] == 0) {   // len na tráve
                float s = stromNoise.GetNoise((float)c, (float)r);
                if (s > 0.6f) {      // len kde je šum dosť vysoký -> zhluky
                    stromy.push_back(sf::Vector2f(c * VELKOST, r * VELKOST));
                }
            }
        }
    }

    sf::RenderWindow window(sf::VideoMode::getDesktopMode(), "RPG");
    sf::View view(sf::FloatRect({0.f, 0.f}, sf::Vector2f(window.getSize())));

    sf::Clock clock;

    sf::RectangleShape hrac(sf::Vector2f(40.f, 40.f));   // štvorec 40x40
    hrac.setFillColor(sf::Color(200, 100, 50));           // oranžová
    hrac.setPosition(sf::Vector2f(400.f, 300.f));         // pozícia

    sf::RectangleShape strom(sf::Vector2f(50.f, 50.f));
    strom.setFillColor(sf::Color(100, 50, 0));

    window.setVerticalSyncEnabled(true);

    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();   // čas od minulého frame v sekundách
        // na ktorej dlaždici je hráč
        int hrac_c = hrac.getPosition().x / VELKOST;
        int hrac_r = hrac.getPosition().y / VELKOST;
        // koľko dlaždíc okolo hráča kresliť (podľa veľkosti okna + rezerva)
        int dosah = 40;

        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                view.setSize(sf::Vector2f(resized->size));   // view = nová veľkosť okna
                window.setView(view);
            }
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
            pohyb(hrac, sf::Vector2f(0.f, -rychlost * dt), mapa, VELKOST);
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
            pohyb(hrac, sf::Vector2f(-rychlost * dt, 0.f), mapa, VELKOST);
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
            pohyb(hrac, sf::Vector2f(0.f, rychlost * dt), mapa, VELKOST);
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
            pohyb(hrac, sf::Vector2f(rychlost * dt, 0.f), mapa, VELKOST);
        }

        view.setCenter(hrac.getPosition());
        window.setView(view);
        window.clear(sf::Color(30, 30, 40));

        for (int r = hrac_r - dosah; r <= hrac_r + dosah; r++) { // kreslenie mapy
            for (int c = hrac_c - dosah; c <= hrac_c + dosah; c++) {
                // ochrana: nekresli mimo mapy
                if (r < 0 || c < 0 || r >= VYSKA_MAPY || c >= SIRKA_MAPY) continue;
                    sf::RectangleShape dlazdica(sf::Vector2f(VELKOST, VELKOST));
                    if (mapa[r][c] == 0) dlazdica.setFillColor(sf::Color(80, 160, 80));
                    if (mapa[r][c] == 1) dlazdica.setFillColor(sf::Color(50, 150, 255));
                    if (mapa[r][c] == 2) dlazdica.setFillColor(sf::Color(160, 160, 160));
                    dlazdica.setPosition(sf::Vector2f(float(c * VELKOST), float(r * VELKOST)));  // ako pri hracovi a strome
                    window.draw(dlazdica);
            }
        }

        sf::Vector2f hracPos = hrac.getPosition();
        for (sf::Vector2f pozicia : stromy) {
            // preskoč stromy ďaleko od hráča
            if (abs(pozicia.x - hracPos.x) > 1500 || abs(pozicia.y - hracPos.y) > 1500) continue;
            strom.setPosition(pozicia);
            window.draw(strom);
        }

        window.draw(hrac);
        window.display();
    }

    return 0;
}