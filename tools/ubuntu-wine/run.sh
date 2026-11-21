#!/usr/bin/env bash
set -euo pipefail

fail() {
    printf '%s\n' "$*" >&2
    exit 1
}

for command in wine wineboot wineserver rsync cabextract flock; do
    command -v "$command" >/dev/null || fail "Не найдена команда $command. См. docs/ubuntu-wine.md."
done

source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)
directx_cab="$source_dir/dx/Mar2009_d3dx9_41_x86.cab"
for file in "$source_dir/v2game.exe" "$source_dir/dinput8.dll" "$directx_cab"; do
    [[ -f "$file" ]] || fail "Отсутствует файл сборки $file"
done

runtime_dir=${VIC2_WINE_DIR:-${XDG_DATA_HOME:-$HOME/.local/share}/v2game/ubuntu-wine}
umask 077
mkdir -p -- "$runtime_dir"
runtime_dir=$(cd -- "$runtime_dir" && pwd -P)
export WINEPREFIX="$runtime_dir/prefix" WINEARCH=win32
export WINEDEBUG=${WINEDEBUG:--all,err+all}

# Пока игра открыта, второй запуск не должен менять её файлы и профиль.
exec 9>"$runtime_dir/launch.lock"
flock -n 9 || fail "Victoria II уже запущена с префиксом $WINEPREFIX"

if [[ ! -f "$WINEPREFIX/.v2game-ready" ]]; then
    WINEDLLOVERRIDES='mscoree,mshtml=' wineboot -u 9>&-
    # Wine может записать реестр и завершить настройку после выхода wineboot.
    wineserver -w 9>&-
    [[ -f "$WINEPREFIX/system.reg" ]] || fail "Wine не создал префикс $WINEPREFIX"
    # Wine по умолчанию связывает папки Windows с домашними папками Linux.
    for profile_dir in "$WINEPREFIX"/drive_c/users/*/; do
        for folder in "$profile_dir"*; do
            if [[ -L "$folder" ]]; then
                unlink -- "$folder"
                mkdir -- "$folder"
            fi
        done
    done
    if [[ -L "$WINEPREFIX/dosdevices/z:" ]]; then
        unlink -- "$WINEPREFIX/dosdevices/z:"
    fi
    touch -- "$WINEPREFIX/.v2game-ready"
fi

game_dir="$WINEPREFIX/drive_c/v2game"
user_dir="$WINEPREFIX/drive_c/v2game-userdata"
mkdir -p -- "$game_dir/Screenshots" "$user_dir/save games"

# Копируются только игровые входы. Производные файлы остаются в префиксе.
filters=(
    --exclude='/gfx/cache/***'
    --include='/map/cache/' --include='/map/cache/time' --exclude='/map/cache/***'
    --exclude='/z_CreatineCore/Cache/***' --exclude='/z_CreatineCore/Logs/***'
    --exclude='/d3dx9_41.dll' --exclude='/userdir.txt'
    --include='/*.dll' --include='/v2game.exe' --include='/victoria2.exe'
    --include='/Victoria2.ico' --include='/launcher.html' --include='/load_bg.jpg'
    --include='/*.txt'
)
for directory in assets battleplans common decisions events gfx history interface inventions launcher \
    localisation map mod music news poptypes script sound technologies tutorial units z_CreatineCore; do
    filters+=("--include=/$directory/***")
done
rsync -rt --checksum --delay-updates --delete-delay "${filters[@]}" --exclude='/*' -- "$source_dir/" "$game_dir/"

printf 'C:/v2game-userdata/' > "$game_dir/userdir.txt"
if [[ ! -e "$user_dir/settings.txt" ]]; then
    # Задать оконный режим при первом запуске, сохраняя байты русских строк.
    LC_ALL=C sed -e 's/^fullScreen=yes/fullScreen=no/' -e 's/^borderless=yes/borderless=no/' \
        "$game_dir/settings.txt" > "$user_dir/settings.txt.new"
    mv -- "$user_dir/settings.txt.new" "$user_dir/settings.txt"
fi
if [[ ! -e "$user_dir/messagetypes_custom.txt" && -f "$game_dir/messagetypes_custom.txt" ]]; then
    cp -- "$game_dir/messagetypes_custom.txt" "$user_dir/messagetypes_custom.txt"
fi

# Встроенная D3DX в Wine 10.0 не компилирует один из шейдеров этой сборки.
cabextract -q -p -F d3dx9_41.dll -- "$directx_cab" > "$game_dir/d3dx9_41.dll.new"
[[ -s "$game_dir/d3dx9_41.dll.new" ]] || fail "Не удалось извлечь d3dx9_41.dll из $directx_cab"
mv -- "$game_dir/d3dx9_41.dll.new" "$game_dir/d3dx9_41.dll"

printf 'Пользовательский каталог Victoria II — %s\n' "$user_dir"
printf 'Журнал Wine — %s\n' "$runtime_dir/wine.log"
cd -- "$game_dir"
export WINEDLLOVERRIDES='dinput8=n,b;d3dx9_41=n;mscoree,mshtml='
wine 'C:\v2game\v2game.exe' "$@" 9>&- > "$runtime_dir/wine.log" 2>&1 || {
    status=$?
    printf 'Wine завершился с кодом %s. См. %s/wine.log\n' "$status" "$runtime_dir" >&2
    exit "$status"
}
