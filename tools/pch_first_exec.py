"""Diagnostic builds: make every generated function report its first run (app/src/ds2_first_exec.cpp).

    python tools/pch_first_exec.py app/generated/default/ds2_pch.h          patch
    python tools/pch_first_exec.py --undo app/generated/default/ds2_pch.h   restore

Appends a redefinition of REX_FUNC_PROLOGUE to the generated precompiled header: the original check, then a static
flag per function and, the first time, RexFirstExec(__func__). One byte load and a branch a call. Every generated
file includes the header, so both ways rebuild all the generated code. A new codegen run writes the header afresh
(unpatched)."""
import sys

BEGIN = '// ds2-nx first-exec trace (tools/pch_first_exec.py) {'
END = '// } ds2-nx first-exec trace'
BLOCK = f'''{BEGIN}
extern "C" void RexFirstExec(const char* func);
#undef REX_FUNC_PROLOGUE
#define REX_FUNC_PROLOGUE()                                       \\
  do {{                                                            \\
    if (((size_t)base & 0x1F) != 0) __builtin_unreachable();      \\
    static bool rex_first_exec_seen_;                             \\
    if (__builtin_expect(!rex_first_exec_seen_, 0)) {{             \\
      rex_first_exec_seen_ = true;                                \\
      RexFirstExec(__func__);                                     \\
    }}                                                             \\
  }} while (0)
{END}
'''


def main():
    args = sys.argv[1:]
    undo = '--undo' in args
    for path in (a for a in args if a != '--undo'):
        text = open(path, encoding='utf-8').read()
        if BEGIN in text:
            start = text.index(BEGIN)
            text = text[:start] + text[text.index(END, start) + len(END) + 1:]
        if not undo:
            # At the very end, after the original definitions (the header is #pragma once, no include guard).
            text = text.rstrip('\n') + '\n\n' + BLOCK
        open(path, 'w', encoding='utf-8', newline='\n').write(text)
        print(('restored ' if undo else 'patched ') + path)


main()
