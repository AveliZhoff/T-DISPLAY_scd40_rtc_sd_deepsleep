#!/usr/bin/env bash
# Зафиксировать текущие доработки и отправить их на GitHub одной командой.
#
# Использование:
#   scripts/sync.sh "краткое описание изменения"
#   scripts/sync.sh                 # описание будет собрано из имён изменённых файлов
#
# Скрипт: 1) проверяет, что мы в git-репозитории; 2) добавляет все изменения;
#         3) делает коммит (если есть что коммитить); 4) пушит ветку в origin;
#         5) печатает ссылки на коммит, сравнение и страницу создания PR.
set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$(pwd)"

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "Ошибка: $ROOT не является git-репозиторием" >&2
  exit 1
fi

BRANCH="$(git rev-parse --abbrev-ref HEAD)"
MSG="${1:-}"

git add -A

if git diff --cached --quiet; then
  echo "Нет новых изменений — коммитить нечего."
else
  if [ -z "$MSG" ]; then
    FILES="$(git diff --cached --name-only | head -5 | paste -sd ', ' -)"
    COUNT="$(git diff --cached --name-only | wc -l | tr -d ' ')"
    MSG="Обновление: $FILES"
    [ "$COUNT" -gt 5 ] && MSG="$MSG и ещё $((COUNT - 5)) файл(ов)"
  fi
  git commit -m "$MSG"
fi

git push origin "$BRANCH"

REPO="$(git remote get-url origin \
  | sed -e 's#^git@github.com:#https://github.com/#' -e 's#\.git$##')"
SHA="$(git rev-parse --short HEAD)"

echo
echo "Ветка:    $BRANCH"
echo "Коммит:   $REPO/commit/$SHA"
echo "Изменения: $REPO/compare/develop...$BRANCH"
echo "PR:       $REPO/compare/develop...$BRANCH?expand=1"
