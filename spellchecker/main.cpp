#include <SymSpell.h>
#include <iostream>
#include <fcntl.h>
#include <io.h>
#include <string>
#include <vector>

int wmain(int argc, wchar_t* argv[])
{
    _setmode(_fileno(stdout), _O_U8TEXT);

    if (argc < 3)
    {
        std::wcerr << L"Usage: spellchecker_cli <dictionary_path> <word1> [word2] ..." << std::endl;
        return 1;
    }

    // Конвертируем путь к словарю из wchar_t в char (UTF-8)
    std::string dicPath;
    for (int i = 0; argv[1][i] != L'\0'; i++)
        dicPath += (char)argv[1][i];

    // Загружаем словарь
    SymSpell symSpell(108558, 2, 3, 1, 5);

    if (!symSpell.LoadDictionary(dicPath, 0, 1, XL(' ')))
    {
        std::wcerr << L"FAILED to load dictionary: " << argv[1] << std::endl;
        return 1;
    }

    // Обрабатываем каждое слово (начиная со 2-го аргумента)
    for (int i = 2; i < argc; i++)
    {
        std::wstring word = argv[i];
        auto suggestions = symSpell.Lookup(word, Verbosity::Top, 2);

        if (!suggestions.empty())
            std::wcout << suggestions[0].term;
        else
            std::wcout << word;

        if (i < argc - 1)
            std::wcout << L" ";
    }

    std::wcout << std::endl;
    return 0;
}