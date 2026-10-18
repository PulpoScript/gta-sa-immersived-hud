# Registro de cambios

Formato basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/). Aún no hay releases publicadas.

## [Sin publicar]

### 0.1.0-dev.2 — Diagnóstico de direcciones (sin probar)

- F10 escribe `ImmersivedHud.log` junto a `gta_sa.exe` con los bytes, el puntero y el float de cada dirección candidata a posición del HUD.
- Solo lee la memoria del juego; no cambia nada.
- Siguiente objetivo acordado: montaje compacto arriba a la derecha (icono en la esquina; a su izquierda hora, dinero, blindaje y salud; estrellas debajo) en el que los elementos ocultos dejan su hueco y el resto se recoloca.

### 0.1.0-dev.1 — Proyecto mínimo (probado)

- Proyecto de Visual Studio basado en plugin-sdk (`Plugin_SA`, Release).
- F11 enciende y apaga el plugin y lo anuncia con `CHud::SetHelpMessage`.
- Al compilar, el `.asi` se copia a `GTA_SA_DIR`.
