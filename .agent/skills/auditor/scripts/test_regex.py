import re
import sys

try:
    # Block comment: /* ... */ using (?s) for dot-matches-all
    # Regex: (?s:/\*.*\*/)
    # Python str: r'(?s:/\*.*?\*/)'
    # Write file: r'(?s:/\*.*?\*/)'
    p_block = r'(?s:/\*.*?\*/)'
    
    # Line comment: // ...
    # Regex: //.*
    p_line = r'//.*'
    
    # String literal: "(?:\\.|[^\\"])*"
    # Regex: "(?:\\.|[^\\"])*"
    # Python str: r'"(?:\\.|[^\\"])*"'
    # Write file: r'"(?:\\\\.|[^\\\\\"])*"'
    p_str = r'"(?:\\\\.|[^\\\\\"])*"'
    
    # Char literal
    p_char = r"'(?:\\\\.|[^\\\\\'])*'"

    full_pattern = f'({p_block})|({p_line})|({p_str})|({p_char})'
    print(f"Pattern: {full_pattern}")
    
    compiled = re.compile(full_pattern)
    print("Compilation SUCCESS")
    
    # Test
    test_str = 'int a = 1; // comment\n/* block \n comment */\nchar* s = "string with \"quote\"";'
    result = compiled.sub(lambda m: ' ' * len(m.group(0)), test_str)
    print("Result length match:", len(result) == len(test_str))
    
except Exception as e:
    print(f"Compilation FAILED: {e}")
    sys.exit(1)