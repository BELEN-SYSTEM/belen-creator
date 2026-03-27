# Belén Creator

Aplicación de **escritorio** para el ecosistema **BetlemSystem**: gestiona el inventario y la lógica de un belén (figuras, ubicaciones, automatización por tipos y puertos, timelines, etc.) con **datos en la nube** vía **Supabase** (Auth + API REST + Row Level Security en PostgreSQL).

## ¿Qué hace?

Tras iniciar sesión, el usuario accede a un panel con secciones para:

- **Piezas** — catálogo de piezas (código, notas, propietario, ubicación, galería, relación con tipos configurables).
- **Timelines** — secuencias de uso / acciones ligadas a piezas y puertos.
- **Propietarios** y **Ubicaciones** — metadatos del inventario.
- **Dispositivos** — referencias hardware (p. ej. Bluetooth).
- **Tipos** y **Puertos** — modelado de comportamientos y pines/parámetros para integrar automatización.

La seguridad de los datos no depende del cliente: las políticas **RLS** en la base definen qué puede ver o modificar cada usuario autenticado.

## Stack técnico

| Área        | Tecnología                          |
|------------|--------------------------------------|
| Cliente    | C++20, **Qt 6** (Widgets, Network)  |
| Backend    | **Supabase** (Auth + PostgREST)      |
| Build      | **CMake** + **Ninja**                |
| Plataforma | Orientado a **Windows** (MSYS2 UCRT64) |

El ejecutable del proyecto se genera como `Editor`; el instalador puede publicarlo como *Belen Creator* (ver `installer.iss` y `scripts/prepare_installer.bat`).

## Arranque rápido (desarrollo)

1. **Supabase** — Crea un proyecto y define `SUPABASE_URL` y `SUPABASE_ANON_KEY` (variables de entorno o `supabase.ini` junto al `.exe`; ver comentarios en el código de configuración).
2. **Esquema SQL** — Scripts de referencia bajo `storage/ddbb/` (`schema/`, `rls/`, `seed/`).
3. **Compilar** — Con MSYS2/Qt en el prefijo esperado (`CMakeLists.txt` usa por defecto `C:/msys64/ucrt64`) o ajusta `CMAKE_PREFIX_PATH`. Puedes usar el preset `mingw` de `CMakePresets.json` y compilar en `build/`.

Para empaquetar dependencias Qt en Windows, el flujo previsto pasa por `scripts/prepare_installer.bat` (incluye `windeployqt` y copia de DLLs de runtime).

## Estructura del repositorio (resumen)

- `src/` — Código de la aplicación (UI, servicios, cliente Supabase).
- `storage/ddbb/` — Definición y políticas de la base de datos.
- `scripts/` — Utilidades de build e instalador.
- `resources.qrc` / `storage/images/` — Recursos embebidos.

---

*Proyecto BetlemSystem · BelenCreator*
