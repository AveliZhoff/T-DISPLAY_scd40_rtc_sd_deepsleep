#!/usr/bin/env bash
# Установить git-хук, который автоматически отправляет каждый коммит на GitHub.
#
# После установки любая доработка, зафиксированная коммитом, сразу оказывается
# в origin — владельцу не нужно просить "запушь", достаточно открыть ссылку.
#
# Использование: scripts/install-hooks.sh
set -euo pipefail

cd "$(dirname "$0")/.."
GIT_DIR="$(git rev-parse --git-dir)"

cat > "$GIT_DIR/hooks/post-commit" <<'HOOK'
#!/usr/bin/env bash
# Автопуш текущей ветки после каждого коммита (устанавливается scripts/install-hooks.sh).
# Отключить разово: AUTO_PUSH=0 git commit ...  |  убрать: rm .git/hooks/post-commit
[ "${AUTO_PUSH:-1}" = "0" ] && exit 0
branch="$(git rev-parse --abbrev-ref HEAD)"
if git push --quiet origin "$branch" 2>/dev/null; then
  url="$(git remote get-url origin | sed -e 's#^git@github.com:#https://github.com/#' -e 's#\.git$##')"
  echo "[auto-push] $branch -> ${url}/commit/$(git rev-parse --short HEAD)"
else
  echo "[auto-push] не удалось отправить $branch в origin (проверьте SSH-доступ)" >&2
fi
exit 0
HOOK

chmod +x "$GIT_DIR/hooks/post-commit"
echo "Готово: .git/hooks/post-commit установлен (ветка пушится после каждого коммита)."
