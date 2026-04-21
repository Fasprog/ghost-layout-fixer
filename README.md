# Ghost Layout Fixer

CLI-утилита для Windows 10/11 для поиска и удаления призрачных раскладок клавиатуры.

Призрачная раскладка — это раскладка, которая видна в переключателе языка, но отсутствует в обычных настройках Windows и может возвращаться после перезагрузки.

## Возможности

- поиск призрачных раскладок через `scan`
- безопасная проверка без изменений через `fix --layout <language-tag> --dry-run`
- исправление через `fix --layout <language-tag>`
- резервное копирование веток реестра через `backup`
- восстановление из `.reg` файла через `restore --file <path>`

## Требования

- права администратора для `backup`, `fix`, `restore`
- PowerShell
- `reg.exe`

## Сборка

```bash
cmake -S . -B build
cmake --build build --config Release
```

## Использование

### Help
```bash
ghost-layout-fixer --help
ghost-layout-fixer -h
```
### Поиск призрачных раскладок
```bash
ghost-layout-fixer scan
```
### Проверка исправления без изменений
```bash
ghost-layout-fixer fix --layout en-GB --dry-run
```
### Исправление
```bash
ghost-layout-fixer fix --layout en-GB
```
### Создание backup
```bash
ghost-layout-fixer backup
```
### Восстановление
```bash
ghost-layout-fixer restore --file .\backups\ghost-layout-backup-YYYYMMDD-HHMMSS.reg
```
