// #####################################################################
//  Immersived HUD — plugin ASI para GTA San Andreas PC 1.0 US
//  Versión: 0.1.0-dev.15 — HUD dinámico (candidata a 0.1.0)
//  Requiere: plugin-sdk (Plugin_SA) y un cargador de ASI.
//  Repositorio: https://github.com/PulpoScript/gta-sa-immersived-hud
// #####################################################################
//
//  QUÉ HACE
//    El HUD va en un montaje compacto arriba a la derecha (icono en la
//    esquina; a su izquierda hora, dinero, blindaje y salud; estrellas
//    debajo; oxígeno abajo a la derecha). Cada elemento aparece cuando
//    pasa algo y se oculta al rato; lo oculto deja su hueco y el resto
//    se recoloca. Las reglas son las del Modo 2 del mod CLEO.
//
//  CONTROLES
//    "."  Toque corto: la hora 5 s. Mantener 0,8 s: todo el HUD en su
//         posición de fábrica; al soltar, todo visible 3 s.
//    F11  Apaga / enciende el plugin (empieza encendido). Apagado: HUD
//         de fábrica.
//    F10  Escribe ImmersivedHud.log junto a gta_sa.exe (diagnóstico,
//         solo lectura).
//
//  TÉCNICA
//    Posición y tamaño: cada coordenada se lee con "fmul [puntero]". No
//    se toca la constante del juego (algunas se comparten): se cambia el
//    puntero de la instrucción para que apunte a un float propio, como
//    hace WidescreenFix. Ocultar: parches "ret" / NOP probados en el mod
//    CLEO. Antes de escribir se comprueba siempre el valor original.
// #####################################################################

#include "plugin.h"
#include "CHud.h"
#include "CWorld.h"
#include "CTimer.h"
#include <cstdio>
#include <cstring>

using namespace plugin;

// =====================================================================
//  CONFIGURACIÓN
// =====================================================================
static const char* VERSION = "0.1.0-dev.15";
static const int   TECLA_ACTIVAR = VK_F11;
static const int   TECLA_DIAGNOSTICO = VK_F10;
static const int   TECLA_CONSULTA = VK_OEM_PERIOD;   // "." (no la del teclado numérico)

static const unsigned int TIEMPO_VISIBLE     = 5000;  // ms: dinero, arma, barras y hora
static const unsigned int TIEMPO_RADAR       = 8000;  // ms tras el último motivo
static const unsigned int TIEMPO_TRAS_SOLTAR = 3000;  // ms: al activar y al soltar "." larga
static const unsigned int PULSACION_LARGA    = 800;   // ms para que "." muestre todo
static const float        SALUD_BAJA         = 25.0f; // puntos: por debajo, barras fijas

// =====================================================================
//  DIRECCIONES A REVISAR (SIN VERIFICAR)
//  Cada una debería ser el operando de 4 bytes de una instrucción que
//  lee un float (por ejemplo fmul [mem] = D8 0D, fld [mem] = D9 05).
//  Fuentes: listas de foros (ugbase.eu, blast.hk) y WidescreenFix
//  (Legacy.ixx). Ver docs/INVESTIGACION.md del mod CLEO.
// =====================================================================
struct Candidata {
    const char* nombre;
    unsigned int direccion;
};

static const Candidata CANDIDATAS[] = {
    // ---- Punteros de posición (listas de foros) ----
    { "Dinero X",             0x58F5FC },
    { "Dinero Y",             0x58F5DC },
    { "Salud X",              0x58EE87 },
    { "Salud Y",              0x58EE68 },
    { "Blindaje X",           0x58EF59 },
    { "Blindaje Y",           0x58EF3A },
    { "Oxigeno X",            0x58F11F },
    { "Estrellas X",          0x58DD0F },
    { "Estrellas Y",          0x58DDFC },
    { "Arma X (a)",           0x58F927 },
    { "Arma X (b)",           0x58F92F },  // WidescreenFix lo redirige
    { "Municion X",           0x58F9F7 },  // WidescreenFix lo redirige
    { "Municion Y",           0x58F9DC },

    // ---- Instrucciones de escala (WidescreenFix), operando en +2 ----
    { "Hora ancho 1",         0x58EB3F + 2 },
    { "Hora ancho 2",         0x58EC0C + 2 },
    { "Hora alto 1",          0x58EB29 + 2 },
    { "Hora alto 2",          0x58EBF9 + 2 },
    { "Dinero ancho 1",       0x58F55C + 2 },
    { "Dinero ancho 2",       0x58F5F4 + 2 },
    { "Dinero alto 1",        0x58F546 + 2 },
    { "Dinero alto 2",        0x58F5CE + 2 },
};

// ---- Rangos de código donde buscar posiciones ----
struct Rango {
    const char* nombre;
    unsigned int inicio;
    unsigned int fin;
};

static const Rango BUSQUEDAS[] = {
    { "hora",              0x58EA80, 0x58EC80 },  // alrededor de las escalas de la hora
    { "icono y municion",  0x58F880, 0x58FAC0 },  // alrededor de las escalas del icono y la munición
    { "estrellas",         0x58DC80, 0x58E020 },  // alrededor de las escalas de las estrellas
    { "dinero",            0x58F500, 0x58F620 },  // alrededor de las escalas del dinero
    { "salud y blindaje",  0x58EE00, 0x58F000 },  // alrededor de las escalas de las barras
    { "dibujo del icono",  0x58D840, 0x58D980 },  // alrededor de las escalas del dibujo del icono
    { "DrawBarChart",      0x728640, 0x728900 },  // CSprite2d::DrawBarChart: borde negro de las barras
};

// ---- Rango a volcar en hexadecimal: CSprite2d::DrawBarChart ----
static const unsigned int VOLCADO_INICIO = 0x728640;
static const unsigned int VOLCADO_FIN    = 0x728A40;

// =====================================================================
//  LECTURA SEGURA
// =====================================================================
// Devuelve true si se pueden leer 'tam' bytes en 'dir' sin fallo.
static bool SePuedeLeer(unsigned int dir, unsigned int tam) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<void*>(dir), &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    unsigned int fin = reinterpret_cast<unsigned int>(mbi.BaseAddress) + mbi.RegionSize;
    return dir + tam <= fin;
}

// Datos del jugador que se está controlando (CWorld::Players[PlayerInFocus]).
static CPlayerInfo& Jugador() {
    return CWorld::Players[static_cast<unsigned char>(CWorld::PlayerInFocus)];
}

// =====================================================================
//  REDIRECCIÓN DE PUNTEROS
// =====================================================================
// Un puntero de coordenada: dónde está, a qué apunta de fábrica y el
// float propio al que se redirige.
struct Coordenada {
    unsigned int direccion;    // operando de 4 bytes de la instrucción
    unsigned int original;     // puntero de fábrica (verificado en dev.2)
    float valor;               // valor propio (unidades del HUD)
    bool redirigida;
};

// Escribe 4 bytes en el código del juego. Devuelve true si se pudo.
static bool EscribirPuntero(unsigned int dir, unsigned int valor) {
    DWORD antes;
    void* p = reinterpret_cast<void*>(dir);
    if (!VirtualProtect(p, 4, PAGE_EXECUTE_READWRITE, &antes)) return false;
    *reinterpret_cast<unsigned int*>(dir) = valor;
    VirtualProtect(p, 4, antes, &antes);
    return true;
}

// Apunta la instrucción al float propio, solo si el puntero es el de fábrica.
static void Redirigir(Coordenada& c) {
    if (c.redirigida) return;
    if (*reinterpret_cast<const unsigned int*>(c.direccion) != c.original) return;
    c.redirigida = EscribirPuntero(c.direccion, reinterpret_cast<unsigned int>(&c.valor));
}

// Devuelve la instrucción a su puntero de fábrica, solo si lo cambiamos nosotros.
static void Restaurar(Coordenada& c) {
    if (!c.redirigida) return;
    if (*reinterpret_cast<const unsigned int*>(c.direccion) != reinterpret_cast<unsigned int>(&c.valor)) return;
    if (EscribirPuntero(c.direccion, c.original)) c.redirigida = false;
}

// =====================================================================
//  MONTAJE COMPACTO
//  Unidades del HUD (con WidescreenFix a 1080p, 1 unidad ≈ 1,93 px).
//  X se mide desde el borde derecho; Y desde arriba.
//  Punteros de fábrica verificados en dev.2, dev.4, dev.6 y dev.8.
//  Las posiciones de la columna y las estrellas se recalculan en cada
//  frame (ActualizarPosiciones): lo que está oculto deja su hueco y el
//  resto sube; sin icono, la columna se pega a la esquina.
// =====================================================================
// ---- Icono de arma y munición (fábrica X 32, Y 20; munición Y = 20 + 43) ----
static Coordenada iconoX     = { 0x58F927, 0x0085950C, -57.0f, false };  // X = ancho - (esto + parte de WidescreenFix)
static Coordenada iconoY     = { 0x58F913, 0x00858BA4,   4.5f, false };
static Coordenada municionX  = { 0x58FA02, 0x0085950C, -57.0f, false };
static Coordenada municionY  = { 0x58F9DC, 0x00858BA4,   4.5f, false };  // Y base
// ---- Hora (fábrica X 32, Y 22, letra 1,1 / 0,55) ----
static Coordenada horaX      = { 0x58EC16, 0x0085950C,  57.0f, false };
static Coordenada horaY      = { 0x58EC04, 0x00858F8C,  12.0f, false };
static Coordenada horaAlto   = { 0x58EB31, 0x00858F14,  0.64f, false };  // alto de letra
static Coordenada horaAncho  = { 0x58EB47, 0x00866CAC,  0.32f, false };  // ancho de letra
// ---- Dinero (fábrica X 32, Y 89, letra 1,1 / 0,55) ----
static Coordenada dineroX    = { 0x58F5FC, 0x0085950C,  57.0f, false };
static Coordenada dineroY    = { 0x58F5DC, 0x00866C88,  38.5f, false };
static Coordenada dineroAlto = { 0x58F54E, 0x00858F14,  0.64f, false };
static Coordenada dineroAncho= { 0x58F564, 0x00866CAC,  0.32f, false };
// ---- Blindaje y salud (fábrica X 94 / 141 = borde izquierdo, Y 48 / 77) ----
static Coordenada blindajeX  = { 0x58EF59, 0x00866B78, 119.0f, false };
static Coordenada blindajeY  = { 0x58EF3A, 0x00862D38,  43.0f, false };
static Coordenada saludX     = { 0x58EE87, 0x0086535C, 166.0f, false };
static Coordenada saludY     = { 0x58EE68, 0x00866CA8,  60.0f, false };
// ---- Estrellas (fábrica X 29, Y 114; tamaño 1,21 / 0,605) ----
static Coordenada estrellasX = { 0x58DD0F, 0x00863210,   8.0f, false };
static Coordenada estrellasY1= { 0x58DDFC, 0x00866C5C,  72.0f, false };  // encendidas (fld)
static Coordenada estrellasY2= { 0x58DEF5, 0x00866C5C,  72.0f, false };  // intermitentes
static Coordenada estrellasY3= { 0x58DFB3, 0x00866C5C,  72.0f, false };  // fondo de las vacías
static Coordenada estrellaAlto1 = { 0x58DCAA, 0x00866C64, 1.065f, false };
static Coordenada estrellaAncho1= { 0x58DCC0, 0x00866C60, 0.532f, false };
static Coordenada estrellaAlto2 = { 0x58DD70, 0x00866C64, 1.065f, false };
static Coordenada estrellaAncho2= { 0x58DD86, 0x00866C60, 0.532f, false };
static Coordenada estrellaAlto3 = { 0x58DF5D, 0x00866C64, 1.065f, false };
static Coordenada estrellaAncho3= { 0x58DF79, 0x00866C60, 0.532f, false };
// ---- Oxígeno: abajo a la derecha (fábrica X 94 = borde izquierdo, compartido con Blindaje X; largo 62) ----
static Coordenada oxigenoX   = { 0x58F11F, 0x00866B78,  67.0f, false };  // borde derecho a 5 unidades del borde

static Coordenada* const MONTAJE[] = {
    &iconoX, &iconoY, &municionX, &municionY,
    &horaX, &horaY, &horaAlto, &horaAncho,
    &dineroX, &dineroY, &dineroAlto, &dineroAncho,
    &blindajeX, &blindajeY, &saludX, &saludY,
    &estrellasX, &estrellasY1, &estrellasY2, &estrellasY3,
    &estrellaAlto1, &estrellaAncho1, &estrellaAlto2, &estrellaAncho2, &estrellaAlto3, &estrellaAncho3,
    &oxigenoX,
};

// ---- Medidas de la columna (Y "efectiva": sin el desplazamiento por salud máxima) ----
static const float COLUMNA_X_CON_ICONO = 57.0f;   // borde derecho de hora, dinero y barras
static const float COLUMNA_X_SIN_ICONO = 9.0f;    // sin icono: alineada con el borde derecho del icono
static const float COLUMNA_Y_INICIO    = 12.0f;   // primera fila
static const float PASO_HORA      = 14.5f;        // de la fila de la hora a la siguiente
static const float PASO_DINERO    = 13.5f;
static const float PASO_BLINDAJE  = 10.0f;
static const float PASO_SALUD     = 10.0f;
static const float LARGO_BLINDAJE = 62.0f;        // la X de la barra es su borde izquierdo
static const float LARGO_SALUD    = 109.0f;       // a salud máxima (el borde derecho no cambia)
static const float ESTRELLAS_BAJO_ICONO = 60.0f;  // con icono, las estrellas no suben más de aquí
static const float OXIGENO_Y      = 541.0f;

// ---- Desplazamiento del juego con salud máxima < 101 (ReSA, DrawPlayerInfo / DrawWanted) ----
// El juego resta estas unidades a la Y; se suman para que no se descoloque.
static const float DESPL_DINERO    = 12.0f;
static const float DESPL_ESTRELLAS = 12.0f;
static const float DESPL_SALUD     = 10.0f;
static const float DESPL_BLINDAJE  = 3.0f;
static const float DESPL_OXIGENO   = 6.0f;

// =====================================================================
//  CONTORNO DE LA HORA Y EL DINERO
//  Ambos llaman a CFont::SetEdge(2) (gta-reversed, Hud.cpp). El valor
//  es un entero, así que lo más fino sin quitarlo es 1.
//  Se busca el patrón "push 2; call CFont::SetEdge" dentro del código de
//  cada elemento y solo se cambia el byte del 2 si aparece una única vez.
// =====================================================================
static const unsigned int FUNCION_SET_EDGE = 0x719590;  // CFont::SetEdge (plugin-sdk)
static const unsigned char CONTORNO_FABRICA = 2;
static const unsigned char CONTORNO_PROPIO  = 1;

struct Contorno {
    const char* nombre;
    unsigned int inicio;     // rango del código donde buscar
    unsigned int fin;
    unsigned int byteValor;  // dirección del 2 encontrada (0 = sin buscar o no válida)
    bool cambiado;
};

static Contorno CONTORNOS[] = {
    { "hora",   0x58EA80, 0x58EC80, 0, false },
    { "dinero", 0x58F500, 0x58F6A0, 0, false },
};

// Cuenta las apariciones de "6A 02 E8 <rel32 a SetEdge>" en el rango y
// devuelve la dirección del byte del valor si hay exactamente una.
static unsigned int BuscarSetEdge(const Contorno& c, int* encontrados) {
    *encontrados = 0;
    unsigned int resultado = 0;
    if (!SePuedeLeer(c.inicio, c.fin - c.inicio + 7)) return 0;
    for (unsigned int dir = c.inicio; dir < c.fin; dir++) {
        const unsigned char* b = reinterpret_cast<const unsigned char*>(dir);
        if (b[0] != 0x6A || b[1] != CONTORNO_FABRICA || b[2] != 0xE8) continue;
        int rel = *reinterpret_cast<const int*>(dir + 3);
        if (dir + 7 + rel != FUNCION_SET_EDGE) continue;
        (*encontrados)++;
        resultado = dir + 1;
    }
    return (*encontrados == 1) ? resultado : 0;
}

static bool EscribirByte(unsigned int dir, unsigned char valor) {
    DWORD antes;
    void* p = reinterpret_cast<void*>(dir);
    if (!VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &antes)) return false;
    *reinterpret_cast<unsigned char*>(dir) = valor;
    VirtualProtect(p, 1, antes, &antes);
    return true;
}

static void AplicarContornos() {
    for (Contorno& c : CONTORNOS) {
        if (c.cambiado) continue;
        int encontrados;
        c.byteValor = BuscarSetEdge(c, &encontrados);
        if (c.byteValor == 0) continue;  // no está o está repetido: no se toca
        c.cambiado = EscribirByte(c.byteValor, CONTORNO_PROPIO);
    }
}

static void QuitarContornos() {
    for (Contorno& c : CONTORNOS) {
        if (!c.cambiado) continue;
        if (*reinterpret_cast<const unsigned char*>(c.byteValor) != CONTORNO_PROPIO) continue;
        if (EscribirByte(c.byteValor, CONTORNO_FABRICA)) c.cambiado = false;
    }
}

// =====================================================================
//  ALTO DE LA BARRA DE SALUD
//  gta-reversed (Hud.cpp): RenderHealthBar (0x589270) usa 109 × 9. Se
//  busca dentro de la función una instrucción fmul/fld que lea 9 y solo
//  se redirige si aparece una única vez (probado en dev.12: 0x589358).
//  El largo de la salud aparece dos veces y el blindaje se deja de
//  fábrica (62 × 9): el autor dio por bueno ese resultado.
// =====================================================================
struct Tamano {
    const char* nombre;
    unsigned int inicio;     // rango del código donde buscar
    unsigned int fin;
    float fabrica;           // valor que debe leer la instrucción
    int encontrados;         // resultado de la última búsqueda
    Coordenada c;            // dirección y puntero se rellenan al encontrarla
};

static Tamano TAMANOS[] = {
    { "salud alto", 0x589270, 0x5893B0, 9.0f, 0, { 0, 0, 8.0f, false } },
};

// Busca "fmul [mem]" (D8 0D) o "fld [mem]" (D9 05) cuyo float valga 'valor'.
static void BuscarTamano(Tamano& t) {
    t.encontrados = 0;
    if (!SePuedeLeer(t.inicio, t.fin - t.inicio + 6)) return;
    for (unsigned int dir = t.inicio; dir < t.fin; dir++) {
        const unsigned char* b = reinterpret_cast<const unsigned char*>(dir);
        bool esFloat = (b[0] == 0xD8 && b[1] == 0x0D) || (b[0] == 0xD9 && b[1] == 0x05);
        if (!esFloat) continue;
        unsigned int puntero = *reinterpret_cast<const unsigned int*>(dir + 2);
        if (!SePuedeLeer(puntero, 4)) continue;
        if (*reinterpret_cast<const float*>(puntero) != t.fabrica) continue;
        t.encontrados++;
        t.c.direccion = dir + 2;
        t.c.original = puntero;
    }
}

static void AplicarTamanos() {
    for (Tamano& t : TAMANOS) {
        if (t.c.redirigida) continue;
        BuscarTamano(t);
        if (t.encontrados == 1) Redirigir(t.c);
    }
}

static void QuitarTamanos() {
    for (Tamano& t : TAMANOS) Restaurar(t.c);
}

// =====================================================================
//  BORDE NEGRO DE LAS BARRAS
//  CSprite2d::DrawBarChart (0x728640) calcula cada lado del borde como
//  "resolución × escala × 2" (fild; fmul [escala]; fadd st, st). La
//  escala es de WidescreenFix, así que su dirección no es fija: se lee
//  al aplicar y el float propio vale esa escala × BORDE_FACTOR.
//  DrawBarChart también dibuja otras barras del juego, por eso el cambio
//  solo dura mientras se dibuja el HUD (drawHudEvent before / after).
//  Volcado verificado en dev.11: las 4 instrucciones son D8 0D.
// =====================================================================
static const float BORDE_FACTOR = 0.7f;

struct Borde {
    unsigned int direccion;  // operando del fmul
    unsigned int original;   // puntero leído al aplicar
    float valor;
    bool redirigida;
};

static Borde BORDES[] = {
    { 0x728866, 0, 0.0f, false },  // arriba
    { 0x7288AB, 0, 0.0f, false },  // abajo
    { 0x7288F7, 0, 0.0f, false },  // izquierda
    { 0x728943, 0, 0.0f, false },  // derecha
};

static void AplicarBordes() {
    for (Borde& b : BORDES) {
        if (b.redirigida) continue;
        const unsigned char* op = reinterpret_cast<const unsigned char*>(b.direccion - 2);
        if (op[0] != 0xD8 || op[1] != 0x0D) continue;  // no es fmul [mem]: no se toca
        unsigned int puntero = *reinterpret_cast<const unsigned int*>(b.direccion);
        if (!SePuedeLeer(puntero, 4)) continue;
        float escala = *reinterpret_cast<const float*>(puntero);
        if (!(escala > 0.0f && escala < 0.01f)) continue;  // valor inesperado: no se toca
        b.original = puntero;
        b.valor = escala * BORDE_FACTOR;
        b.redirigida = EscribirPuntero(b.direccion, reinterpret_cast<unsigned int>(&b.valor));
    }
}

static void QuitarBordes() {
    for (Borde& b : BORDES) {
        if (!b.redirigida) continue;
        if (*reinterpret_cast<const unsigned int*>(b.direccion) != reinterpret_cast<unsigned int>(&b.valor)) continue;
        if (EscribirPuntero(b.direccion, b.original)) b.redirigida = false;
    }
}

// =====================================================================
//  OXÍGENO Y
//  Probado en dev.14: 0x58F100 es la Y del oxígeno (ReSA: fmul [0x86503C]
//  en 0x58F0FE). Como el puntero de fábrica no se anotó, se lee al
//  aplicar: solo se redirige si es un fmul [mem] (D8 0D) que lee un
//  float entre 0 y 448.
// =====================================================================
static const unsigned int OXIGENO_Y_DIR = 0x58F100;
static Coordenada oxigenoY = { OXIGENO_Y_DIR, 0, OXIGENO_Y + DESPL_OXIGENO, false };
static float oxigenoYFabrica = -1.0f;   // valor leído (para el log)

// Comprueba la instrucción y rellena el puntero de fábrica. Devuelve true si es válida.
static bool ComprobarOxigenoY() {
    if (!SePuedeLeer(OXIGENO_Y_DIR - 2, 6)) return false;
    const unsigned char* op = reinterpret_cast<const unsigned char*>(OXIGENO_Y_DIR - 2);
    if (op[0] != 0xD8 || op[1] != 0x0D) return false;
    unsigned int puntero = *reinterpret_cast<const unsigned int*>(OXIGENO_Y_DIR);
    if (!SePuedeLeer(puntero, 4)) return false;
    float valor = *reinterpret_cast<const float*>(puntero);
    oxigenoYFabrica = valor;
    if (!(valor > 0.0f && valor < 448.0f)) return false;
    oxigenoY.original = puntero;
    return true;
}

static bool montajeActivo = false;  // montaje compacto aplicado

static void AplicarMontaje() {
    if (montajeActivo) return;
    for (Coordenada* c : MONTAJE) Redirigir(*c);
    if (!oxigenoY.redirigida && ComprobarOxigenoY()) Redirigir(oxigenoY);
    AplicarContornos();
    AplicarTamanos();
    montajeActivo = true;
}

static void QuitarMontaje() {
    if (!montajeActivo) return;
    for (Coordenada* c : MONTAJE) Restaurar(*c);
    Restaurar(oxigenoY);
    QuitarContornos();
    QuitarTamanos();
    montajeActivo = false;
}

// =====================================================================
//  OCULTAR ELEMENTOS (técnicas probadas en el mod CLEO, ver DIRECCIONES.md)
//  - ret (0xC3) al principio de funciones cdecl: icono, munición, salud
//    y blindaje. Quien llama limpia la pila, así que es seguro.
//  - 5 NOP en lugar de "call CFont::PrintString" del dinero (0x58F607).
//  Antes de escribir se comprueban los bytes originales; si no coinciden
//  (otro ejecutable u otro mod), ese elemento nunca se oculta.
// =====================================================================
struct Parche {
    const char* nombre;
    unsigned int direccion;
    unsigned int tam;
    unsigned char original[5];
    unsigned char parche[5];
    int estado;              // 0 = sin comprobar, 1 = disponible, -1 = no disponible
    bool aplicado;
};

static Parche parcheIcono    = { "icono",    0x58D7D0, 1, { 0xA1 }, { 0xC3 }, 0, false };
static Parche parcheMunicion = { "municion", 0x5893B0, 1, { 0x53 }, { 0xC3 }, 0, false };
static Parche parcheSalud    = { "salud",    0x589270, 1, { 0x83 }, { 0xC3 }, 0, false };
static Parche parcheBlindaje = { "blindaje", 0x5890A0, 1, { 0x83 }, { 0xC3 }, 0, false };
static Parche parcheDinero   = { "dinero",   0x58F607, 5, { 0xE8, 0xF4, 0xB0, 0x18, 0x00 },
                                                          { 0x90, 0x90, 0x90, 0x90, 0x90 }, 0, false };

static Parche* const PARCHES[] = { &parcheIcono, &parcheMunicion, &parcheSalud, &parcheBlindaje, &parcheDinero };

static bool EscribirBytes(unsigned int dir, const unsigned char* bytes, unsigned int tam) {
    DWORD antes;
    void* p = reinterpret_cast<void*>(dir);
    if (!VirtualProtect(p, tam, PAGE_EXECUTE_READWRITE, &antes)) return false;
    memcpy(p, bytes, tam);
    VirtualProtect(p, tam, antes, &antes);
    return true;
}

static void Ocultar(Parche& p) {
    if (p.aplicado) return;
    if (p.estado == 0) {
        bool igual = SePuedeLeer(p.direccion, p.tam) &&
                     memcmp(reinterpret_cast<const void*>(p.direccion), p.original, p.tam) == 0;
        p.estado = igual ? 1 : -1;
    }
    if (p.estado != 1) return;
    if (memcmp(reinterpret_cast<const void*>(p.direccion), p.original, p.tam) != 0) return;
    p.aplicado = EscribirBytes(p.direccion, p.parche, p.tam);
}

static void Mostrar(Parche& p) {
    if (!p.aplicado) return;
    if (memcmp(reinterpret_cast<const void*>(p.direccion), p.parche, p.tam) != 0) return;
    if (EscribirBytes(p.direccion, p.original, p.tam)) p.aplicado = false;
}

static void Poner(Parche& p, bool oculto) {
    if (oculto) Ocultar(p);
    else        Mostrar(p);
}

// ---- Hora y radar: indicadores del juego (CHud::bDrawClock, CHud::bScriptDontDisplayRadar) ----
// Las misiones también los usan. Para que nunca vean el cambio del
// plugin, solo se cambian mientras se dibuja el HUD (drawHudEvent
// before) y se devuelven justo después (after), como el borde de las
// barras. En gameProcessEvent solo se decide si hay que ocultarlos.
static bool ocultarHora = false, ocultarRadar = false;   // decisión de este frame
static bool horaQuitada = false, radarQuitado = false;   // cambiados en el dibujo actual

static void OcultarIndicadoresEnDibujo() {
    if (ocultarHora && CHud::bDrawClock) { CHud::bDrawClock = false; horaQuitada = true; }
    if (ocultarRadar && !CHud::bScriptDontDisplayRadar) { CHud::bScriptDontDisplayRadar = true; radarQuitado = true; }
}

static void DevolverIndicadores() {
    if (horaQuitada)  { CHud::bDrawClock = true; horaQuitada = false; }
    if (radarQuitado) { CHud::bScriptDontDisplayRadar = false; radarQuitado = false; }
}

// =====================================================================
//  POSICIONES DINÁMICAS
//  Se usa siempre el montaje completo, pero lo que no se ve deja su
//  hueco: los de debajo suben y, sin icono, la columna se pega a la
//  esquina. Las estrellas van debajo de la columna (y del icono si se ve).
//  Con salud máxima < 101 el juego resta DESPL_* a la Y: se compensa.
// =====================================================================
static void ActualizarPosiciones(CPed* ped) {
    bool saludMaxBaja = Jugador().m_nMaxHealth < 101;
    float d = saludMaxBaja ? 1.0f : 0.0f;

    bool hayIcono = !parcheIcono.aplicado;
    float colX = hayIcono ? COLUMNA_X_CON_ICONO : COLUMNA_X_SIN_ICONO;
    float y = COLUMNA_Y_INICIO;

    // ---- Hora (se dibuja si el juego la tiene activada y el plugin no la oculta) ----
    horaX.valor = colX;
    horaY.valor = y;
    if (CHud::bDrawClock && !ocultarHora) y += PASO_HORA;

    // ---- Dinero ----
    dineroX.valor = colX;
    dineroY.valor = y + DESPL_DINERO * d;
    if (!parcheDinero.aplicado) y += PASO_DINERO;

    // ---- Blindaje (el juego no lo dibuja con blindaje <= 1) ----
    blindajeX.valor = colX + LARGO_BLINDAJE;
    blindajeY.valor = y + DESPL_BLINDAJE * d;
    if (!parcheBlindaje.aplicado && ped && ped->m_fArmour > 1.0f) y += PASO_BLINDAJE;

    // ---- Salud ----
    saludX.valor = colX + LARGO_SALUD;
    saludY.valor = y + DESPL_SALUD * d;
    if (!parcheSalud.aplicado) y += PASO_SALUD;

    // ---- Estrellas ----
    float yEstrellas = (hayIcono && y < ESTRELLAS_BAJO_ICONO) ? ESTRELLAS_BAJO_ICONO : y;
    estrellasY1.valor = estrellasY2.valor = estrellasY3.valor = yEstrellas + DESPL_ESTRELLAS * d;

    // ---- Oxígeno ----
    oxigenoY.valor = OXIGENO_Y + DESPL_OXIGENO * d;
}

// =====================================================================
//  COMPORTAMIENTO DINÁMICO (mismas reglas que el Modo 2 del mod CLEO)
//  - Dinero: 5 s tras un cambio.
//  - Arma y munición: 5 s tras cambiar ranura, tipo o munición total.
//  - Salud y blindaje: 5 s tras un cambio; fijas con salud < 25.
//  - Radar: en vehículo, con nivel de búsqueda o esprintando; se oculta
//    8 s después del último motivo.
//  - Hora: solo con la tecla "." (toque corto: 5 s).
//  - Mantener "." 0,8 s: todo el HUD en su posición de fábrica; al
//    soltar, montaje compacto con todo visible 3 s.
//  - Al activar el plugin (o cargar partida): todo visible 3 s.
//  Los tiempos usan el reloj del juego (se para en pausa).
// =====================================================================
static unsigned int tDinero = 0, tArma = 0, tBarras = 0, tRadar = 0, tHora = 0;

// Deja cada elemento "a 3 s de ocultarse".
static void MostrarTodo3s(unsigned int ahora) {
    tDinero = tArma = tBarras = tHora = ahora - (TIEMPO_VISIBLE - TIEMPO_TRAS_SOLTAR);
    tRadar = ahora - (TIEMPO_RADAR - TIEMPO_TRAS_SOLTAR);
}

static bool Visible(unsigned int ahora, unsigned int desde, unsigned int duracion) {
    return ahora - desde < duracion;
}

// ---- Valores anteriores para detectar cambios ----
struct EstadoArmas {
    unsigned int ranura;          // unsigned int: sin bytes de relleno para memcmp
    unsigned int tipo[13];
    unsigned int municion[13];
};

static bool datosIniciados = false;
static int dineroAntes = 0;
static EstadoArmas armasAntes;
static float saludAntes = 0.0f, blindajeAntes = 0.0f;

static void LeerArmas(CPed* ped, EstadoArmas& e) {
    memset(&e, 0, sizeof(e));
    e.ranura = ped->m_nSelectedWepSlot;
    for (int i = 0; i < 13; i++) {
        e.tipo[i] = static_cast<unsigned int>(ped->m_aWeapons[i].m_eWeaponType);
        e.municion[i] = ped->m_aWeapons[i].m_nAmmoTotal;
    }
}

// Detecta los eventos de este frame y reinicia el tiempo de cada elemento.
static void DetectarEventos(CPed* ped, unsigned int ahora) {
    int dinero = Jugador().m_nMoney;
    if (!ped) return;   // cargando: no hay jugador

    EstadoArmas armas;
    LeerArmas(ped, armas);
    float salud = ped->m_fHealth;
    float blindaje = ped->m_fArmour;

    if (!datosIniciados) {
        // ---- Primera lectura: solo se guardan los valores ----
        dineroAntes = dinero;
        armasAntes = armas;
        saludAntes = salud;
        blindajeAntes = blindaje;
        datosIniciados = true;
        return;
    }

    if (dinero != dineroAntes) { dineroAntes = dinero; tDinero = ahora; }
    if (memcmp(&armas, &armasAntes, sizeof(armas)) != 0) { armasAntes = armas; tArma = ahora; }
    if (salud != saludAntes || blindaje != blindajeAntes) {
        saludAntes = salud;
        blindajeAntes = blindaje;
        tBarras = ahora;
    }

    // ---- Radar: en vehículo, esprintando o con nivel de búsqueda ----
    CWanted* buscado = FindPlayerWanted();
    if (ped->bInVehicle || ped->m_nMoveState == PEDMOVE_SPRINT ||
        (buscado && buscado->m_nWantedLevel > 0)) {
        tRadar = ahora;
    }
}

// =====================================================================
//  DIAGNÓSTICO
// =====================================================================
// Escribe el archivo junto a gta_sa.exe. Devuelve true si se pudo.
static bool EscribirDiagnostico() {
    char ruta[MAX_PATH];
    GetModuleFileNameA(NULL, ruta, MAX_PATH);
    char* barra = strrchr(ruta, '\\');
    if (barra) strcpy(barra + 1, "ImmersivedHud.log");
    else strcpy(ruta, "ImmersivedHud.log");

    FILE* f = fopen(ruta, "w");
    if (!f) return false;

    fprintf(f, "Immersived HUD v%s - diagnostico (solo lectura)\n", VERSION);
    fprintf(f, "Columnas: nombre | direccion | bytes desde dir-2 | puntero | float apuntado\n\n");

    for (const Candidata& c : CANDIDATAS) {
        fprintf(f, "%-16s 0x%06X  ", c.nombre, c.direccion);

        // ---- Bytes alrededor: 2 antes (código de instrucción) y 4 del operando ----
        if (!SePuedeLeer(c.direccion - 2, 6)) {
            fprintf(f, "no se puede leer\n");
            continue;
        }
        const unsigned char* b = reinterpret_cast<const unsigned char*>(c.direccion - 2);
        fprintf(f, "%02X %02X [%02X %02X %02X %02X]  ", b[0], b[1], b[2], b[3], b[4], b[5]);

        // ---- Puntero y float al que apunta ----
        unsigned int puntero = *reinterpret_cast<const unsigned int*>(c.direccion);
        fprintf(f, "-> 0x%08X  ", puntero);
        if (SePuedeLeer(puntero, 4)) {
            float valor = *reinterpret_cast<const float*>(puntero);
            fprintf(f, "= %.6f\n", valor);
        } else {
            fprintf(f, "(no apunta a memoria legible)\n");
        }
    }

    // ---- Búsqueda de instrucciones de float ----
    // Instrucciones x87 con operando de memoria directo:
    //   D8 05 fadd, D8 0D fmul, D8 25 fsub, D8 2D fsubr, D8 35 fdiv,
    //   D8 3D fdivr, D9 05 fld. Solo se anotan si el puntero es legible.
    for (const Rango& r : BUSQUEDAS) {
        fprintf(f, "\nBusqueda 0x%06X-0x%06X (%s)\n", r.inicio, r.fin, r.nombre);
        fprintf(f, "Columnas: direccion del operando | instruccion | puntero | float apuntado\n\n");
        if (!SePuedeLeer(r.inicio, r.fin - r.inicio + 6)) {
            fprintf(f, "no se puede leer el rango\n");
            continue;
        }
        for (unsigned int dir = r.inicio; dir < r.fin; dir++) {
            const unsigned char* b = reinterpret_cast<const unsigned char*>(dir);
            const char* nombre = NULL;
            if (b[0] == 0xD8) {
                switch (b[1]) {
                    case 0x05: nombre = "fadd "; break;
                    case 0x0D: nombre = "fmul "; break;
                    case 0x25: nombre = "fsub "; break;
                    case 0x2D: nombre = "fsubr"; break;
                    case 0x35: nombre = "fdiv "; break;
                    case 0x3D: nombre = "fdivr"; break;
                }
            } else if (b[0] == 0xD9 && b[1] == 0x05) {
                nombre = "fld  ";
            }
            if (!nombre) continue;

            unsigned int puntero = *reinterpret_cast<const unsigned int*>(dir + 2);
            if (!SePuedeLeer(puntero, 4)) continue;
            float valor = *reinterpret_cast<const float*>(puntero);
            fprintf(f, "0x%06X  %s -> 0x%08X = %.6f\n", dir + 2, nombre, puntero, valor);
        }
    }

    // ---- Oxígeno Y ----
    fprintf(f, "\nOxigeno Y (0x%06X)\n\n", OXIGENO_Y_DIR);
    if (oxigenoY.redirigida) {
        fprintf(f, "cambiado a %.1f (fabrica %.1f, puntero 0x%08X)\n", oxigenoY.valor, oxigenoYFabrica, oxigenoY.original);
    } else {
        bool valida = ComprobarOxigenoY();
        const unsigned char* op = reinterpret_cast<const unsigned char*>(OXIGENO_Y_DIR - 2);
        fprintf(f, "bytes %02X %02X  valor %.3f  %s\n", op[0], op[1], oxigenoYFabrica,
                valida ? "valida" : "NO valida (no se toca)");
    }

    // ---- Tamaño de las barras ----
    fprintf(f, "\nTamano de las barras\n\n");
    for (Tamano& t : TAMANOS) {
        if (t.c.redirigida) {
            fprintf(f, "%-15s cambiado a %.1f en 0x%06X (fabrica %.1f)\n", t.nombre, t.c.valor, t.c.direccion, t.fabrica);
            continue;
        }
        BuscarTamano(t);
        fprintf(f, "%-15s encontrados: %d", t.nombre, t.encontrados);
        if (t.encontrados == 1) fprintf(f, "  en 0x%06X", t.c.direccion);
        fprintf(f, "\n");
    }

    // ---- Ocultar: estado de cada parche ----
    fprintf(f, "\nParches para ocultar (0 = sin comprobar, 1 = disponible, -1 = no disponible)\n\n");
    for (const Parche* p : PARCHES) {
        fprintf(f, "%-9s 0x%06X  estado %2d  %s  bytes", p->nombre, p->direccion, p->estado,
                p->aplicado ? "oculto " : "visible");
        if (SePuedeLeer(p->direccion, p->tam))
            for (unsigned int i = 0; i < p->tam; i++)
                fprintf(f, " %02X", reinterpret_cast<const unsigned char*>(p->direccion)[i]);
        fprintf(f, "\n");
    }
    fprintf(f, "hora: ocultar %d (bDrawClock %d)\n", ocultarHora, CHud::bDrawClock);
    fprintf(f, "radar: ocultar %d (bScriptDontDisplayRadar %d)\n", ocultarRadar, CHud::bScriptDontDisplayRadar);
    fprintf(f, "salud maxima: %d\n", Jugador().m_nMaxHealth);

    // ---- Contorno de la hora y el dinero ----
    fprintf(f, "\nContorno (push 2; call CFont::SetEdge 0x%06X)\n\n", FUNCION_SET_EDGE);
    for (const Contorno& c : CONTORNOS) {
        if (c.cambiado) {
            fprintf(f, "%-8s cambiado a %d en 0x%06X\n", c.nombre, CONTORNO_PROPIO, c.byteValor);
            continue;
        }
        int encontrados;
        unsigned int dir = BuscarSetEdge(c, &encontrados);
        fprintf(f, "%-8s encontrados: %d", c.nombre, encontrados);
        if (dir) fprintf(f, "  valor en 0x%06X", dir);
        fprintf(f, "\n");
    }

    // ---- Volcado de CSprite2d::DrawBarChart (para desensamblar fuera) ----
    fprintf(f, "\nVolcado 0x%06X-0x%06X (DrawBarChart)\n\n", VOLCADO_INICIO, VOLCADO_FIN);
    if (SePuedeLeer(VOLCADO_INICIO, VOLCADO_FIN - VOLCADO_INICIO)) {
        for (unsigned int dir = VOLCADO_INICIO; dir < VOLCADO_FIN; dir += 16) {
            const unsigned char* b = reinterpret_cast<const unsigned char*>(dir);
            fprintf(f, "%06X:", dir);
            for (int i = 0; i < 16; i++) fprintf(f, " %02X", b[i]);
            fprintf(f, "\n");
        }
    } else {
        fprintf(f, "no se puede leer el rango\n");
    }

    fclose(f);
    return true;
}

// =====================================================================
//  PLUGIN
// =====================================================================
static bool activo = true;      // el plugin empieza encendido
static bool reinicio = false;   // partida cargada o nueva (Events::reInitGameEvent)

// Deja el juego como de fábrica: posiciones, tamaños y nada oculto.
static void TodoDeFabrica() {
    QuitarMontaje();
    for (Parche* p : PARCHES) Mostrar(*p);
    ocultarHora = ocultarRadar = false;
}

class ImmersivedHud {
public:
    ImmersivedHud() {
        // Se ejecuta una vez por frame mientras se juega.
        Events::gameProcessEvent += [] {
            static bool primerFrame = true;
            static bool teclaAntes = false;      // F11 pulsada en el frame anterior
            static bool diagAntes = false;       // F10 pulsada en el frame anterior
            static bool puntoAntes = false;      // "." pulsada en el frame anterior
            static bool puntoLargo = false;      // "." lleva pulsada 0,8 s o más
            static unsigned int tPunto = 0;      // momento en que se pulsó "."
            static unsigned int ahoraAntes = 0;
            static char texto[96];

            unsigned int ahora = CTimer::m_snTimeInMilliseconds;
            CPed* ped = FindPlayerPed();

            // ---- Inicio o partida cargada: evento del juego o salto del reloj ----
            // (el reloj avanza como mucho 300 ms por frame: un salto mayor es una carga)
            if (primerFrame || reinicio || ahora < ahoraAntes || ahora - ahoraAntes > 1000) {
                primerFrame = false;
                reinicio = false;
                datosIniciados = false;
                MostrarTodo3s(ahora);
            }
            ahoraAntes = ahora;

            // ---- Tecla F11: cambia de estado solo al pulsar (no al mantener) ----
            bool teclaAhora = (GetAsyncKeyState(TECLA_ACTIVAR) & 0x8000) != 0;
            if (teclaAhora && !teclaAntes) {
                activo = !activo;
                if (activo) MostrarTodo3s(ahora);

                // ---- Aviso en pantalla (sin acentos: la fuente del juego no los tiene) ----
                sprintf(texto, "Immersived HUD v%s: %s", VERSION, activo ? "activado" : "desactivado");
                CHud::SetHelpMessage(texto, true, false, false);
            }
            teclaAntes = teclaAhora;

            // ---- Tecla F10: diagnóstico (solo lectura) ----
            bool diagAhora = (GetAsyncKeyState(TECLA_DIAGNOSTICO) & 0x8000) != 0;
            if (diagAhora && !diagAntes) {
                bool ok = EscribirDiagnostico();
                sprintf(texto, ok ? "Diagnostico guardado en ImmersivedHud.log"
                                  : "No se pudo escribir ImmersivedHud.log");
                CHud::SetHelpMessage(texto, true, false, false);
            }
            diagAntes = diagAhora;

            // ---- Eventos del juego (siempre, para no detectar cambios viejos al encender) ----
            DetectarEventos(ped, ahora);

            if (!activo) {
                puntoAntes = puntoLargo = false;
                TodoDeFabrica();
                return;
            }

            // ---- Tecla ".": toque = hora 5 s; mantener 0,8 s = HUD de fábrica ----
            bool puntoAhora = (GetAsyncKeyState(TECLA_CONSULTA) & 0x8000) != 0;
            if (puntoAhora) {
                if (!puntoAntes) tPunto = ahora;
                if (ahora - tPunto >= PULSACION_LARGA) puntoLargo = true;
            } else if (puntoAntes) {
                if (puntoLargo) MostrarTodo3s(ahora);   // todo 3 s, la hora incluida
                else            tHora = ahora;          // la hora 5 s
                puntoLargo = false;
            }
            puntoAntes = puntoAhora;

            if (puntoLargo) {
                TodoDeFabrica();
                return;
            }

            // ---- Estado deseado de cada elemento ----
            bool saludBaja = ped && ped->m_fHealth < SALUD_BAJA;
            bool ocultarArma   = !Visible(ahora, tArma, TIEMPO_VISIBLE);
            bool ocultarBarras = !Visible(ahora, tBarras, TIEMPO_VISIBLE) && !saludBaja;

            AplicarMontaje();
            Poner(parcheDinero, !Visible(ahora, tDinero, TIEMPO_VISIBLE));
            Poner(parcheIcono, ocultarArma);
            Poner(parcheMunicion, ocultarArma);
            Poner(parcheSalud, ocultarBarras);
            Poner(parcheBlindaje, ocultarBarras);
            ocultarHora  = !Visible(ahora, tHora, TIEMPO_VISIBLE);
            ocultarRadar = !Visible(ahora, tRadar, TIEMPO_RADAR);

            ActualizarPosiciones(ped);
        };

        // ---- Partida cargada o nueva: todo visible 3 s ----
        Events::reInitGameEvent += [] {
            reinicio = true;
        };

        // ---- Solo mientras se dibuja el HUD: borde de las barras, hora y radar ----
        Events::drawHudEvent.before += [] {
            if (montajeActivo) AplicarBordes();
            OcultarIndicadoresEnDibujo();
        };
        Events::drawHudEvent.after += [] {
            QuitarBordes();
            DevolverIndicadores();
        };
    }
} immersivedHud;
