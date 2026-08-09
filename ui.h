#pragma once
#include <SFML/Graphics.hpp>
#include <string>

///////////////////////////////////////////////////////////////////////////////////////////////////
//   PALETA A ROZMERY UI - MENIT LEN TU, NIKDE NIZSIE UZ ZIADNE RUCNE CISLA A FARBY
///////////////////////////////////////////////////////////////////////////////////////////////////
const sf::Color UI_TMAVA        = sf::Color(24, 20, 18, 235);   // pozadie panelov
const sf::Color UI_OKRAJ        = sf::Color(120, 100, 60);      // zlatý okraj
const sf::Color UI_TEXT         = sf::Color(235, 228, 210);     // bežný text
const sf::Color UI_ZLATA        = sf::Color(235, 190, 90);      // zvýraznenie (quest, tvoj ťah)
const sf::Color UI_CERVENA      = sf::Color(170, 45, 45);       // HP, útok
const sf::Color UI_TMAVOCERVENA = sf::Color(60, 25, 25);        // pozadie HP baru
const sf::Color UI_MODRA        = sf::Color(55, 65, 105);       // koniec kola
const sf::Color UI_SIVA         = sf::Color(70, 68, 65);        // vypnuté tlačidlo
const sf::Color UI_SIVYTEXT     = sf::Color(140, 138, 135);     // text vypnutého tlačidla
const sf::Color UI_PANEL_SVETLY  = sf::Color(45, 40, 36, 235);   // odpovede v dialógu
const sf::Color UI_NEPRIATEL    = sf::Color(235, 120, 120);     // hlášky a text nepriateľa
const sf::Color UI_ZAVOJ        = sf::Color(0, 0, 0, 160);       // stmavenie obrazovky pod menu
const sf::Color UI_DOSAH         = sf::Color(90, 150, 255, 70);  // dlaždice, kam sa dá dôjsť
const sf::Color UI_DOSAH_OKRAJ   = sf::Color(120, 180, 255, 130);// okraj dosahu pohybu
const sf::Color UI_VYBRANE      = sf::Color(150, 115, 45);      // vybrana zbraň v bare
const sf::Color UI_CIEL_OK      = sf::Color(120, 220, 120);     // dá sa zaútočiť
const sf::Color UI_CIEL_NIE     = sf::Color(220, 80, 80);       // nedá sa

const float ODSTUP = 16.f;            // medzera od okraja a medzi prvkami
const float VYSKA_TLACIDLA = 52.f;
const int PISMO_MALE = 18;
const int PISMO_NORMAL = 22;
const int PISMO_VELKE = 28;


///////////////////////////////////////////////////////////////////////////////////////////////////
//   STAVEBNE KOCKY UI - vsetko ostatne sa kresli len cez tieto tri funkcie
///////////////////////////////////////////////////////////////////////////////////////////////////

// KRESLI PANEL - JEDINE miesto, kde sa kresli pozadie okien, bublin a tlacidiel.
// Az raz pridu pixel-art textury, prepise sa VNUTRO tejto funkcie (9-slice ram)
// a zmeni sa cele UI v hre naraz. Preto tu nikde inde nekresli obdlzniky rucne.
inline void kresliPanel(sf::RenderWindow& window, sf::FloatRect box, sf::Color vypln, sf::Color okraj) {
    sf::RectangleShape tvar(box.size);
    tvar.setPosition(box.position);
    tvar.setFillColor(vypln);
    tvar.setOutlineColor(okraj);
    tvar.setOutlineThickness(2.f);
    window.draw(tvar);
}

// KRESLI TEXT VYCENTROVANY NA BOD
// POZOR (toto bola pricina "rozhadzaneho" UI): getLocalBounds() nezacina v nule,
// ma aj position - odsadenie podla tvaru pismen a stupania fontu. Preto sa NEcentruje
// posuvanim pozicie, ale posunutim KOTVY: setOrigin(tb.position + tb.size/2).
// Odvtedy je uplne jedno, ake pismena v texte su.
inline void kresliTextVStrede(sf::RenderWindow& window, sf::Font& font, const std::string& text,
                       sf::Vector2f stred, int velkost, sf::Color farba, bool obrys = false) {
    sf::Text t(font);
    t.setString(text);
    t.setCharacterSize(velkost);
    t.setFillColor(farba);
    if (obrys) {                                // obrys sa hodí na text vo svete (na tráve)
        t.setOutlineColor(sf::Color::Black);
        t.setOutlineThickness(2.f);
    }
    sf::FloatRect tb = t.getLocalBounds();
    t.setOrigin(tb.position + tb.size / 2.f);
    t.setPosition(stred);
    window.draw(t);
}

// KRESLI TEXT ZAROVNANY VLAVO, ale ZVISLE vycentrovany na dany bod
inline void kresliTextVlavo(sf::RenderWindow& window, sf::Font& font, const std::string& text,
                     sf::Vector2f lavyStred, int velkost, sf::Color farba) {
    sf::Text t(font);
    t.setString(text);
    t.setCharacterSize(velkost);
    t.setFillColor(farba);
    sf::FloatRect tb = t.getLocalBounds();
    t.setOrigin(sf::Vector2f(tb.position.x, tb.position.y + tb.size.y / 2.f));
    t.setPosition(lavyStred);
    window.draw(t);
}

// KRESLI BAR (pozadie + vypln podla podielu 0.0 az 1.0) - HP hraca aj HP vlka
inline void kresliBar(sf::RenderWindow& window, sf::FloatRect box, float podiel,
               sf::Color farba, sf::Color pozadie) {
    if (podiel < 0.f) podiel = 0.f;   // poistka, nech sa nekresli zaporna sirka
    if (podiel > 1.f) podiel = 1.f;

    sf::RectangleShape pozadieTvar(box.size);
    pozadieTvar.setPosition(box.position);
    pozadieTvar.setFillColor(pozadie);
    pozadieTvar.setOutlineColor(UI_OKRAJ);
    pozadieTvar.setOutlineThickness(1.f);
    window.draw(pozadieTvar);

    sf::RectangleShape vypln(sf::Vector2f(box.size.x * podiel, box.size.y));
    vypln.setPosition(box.position);
    vypln.setFillColor(farba);
    window.draw(vypln);
}

// KRESLI TLACIDLO - uz nerobi nic vlastne, len poskladat panel + text
inline void kresliTlacidlo(sf::RenderWindow& window, sf::Font& font, sf::FloatRect box,
                    const std::string& popis, sf::Color farba, bool aktivne) {
    sf::Color vypln = farba;
    sf::Color textFarba = UI_TEXT;
    if (!aktivne) {                    // akcia už minutá -> zošedne aj rámik aj text
        vypln = UI_SIVA;
        textFarba = UI_SIVYTEXT;
    }
    kresliPanel(window, box, vypln, UI_OKRAJ);
    kresliTextVStrede(window, font, popis, box.position + box.size / 2.f, PISMO_NORMAL, textFarba);
}

// KRESLI HLASKU NAD POSTAVOU (vo svete, nie v UI).
// Berie STRED postavy (nie lavy horny roh), aby tu nebolo natvrdo napisane +20.
// vyskaNad = ako vysoko nad stredom, nech sa hlaska vlka nebije s jeho HP barom.
inline void kresliHlasku(sf::RenderWindow& window, sf::Font& font, const std::string& text,
                  sf::Vector2f stredPostavy, sf::Color farba, float vyskaNad) {
    kresliTextVStrede(window, font, text, sf::Vector2f(stredPostavy.x, stredPostavy.y - vyskaNad),
                      PISMO_NORMAL, farba, true);
}
