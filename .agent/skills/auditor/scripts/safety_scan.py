import os
import sys
import re

# Enhanced Safety Scanner for NoMoreDay
# Checks for memory safety, concurrency risks, and project-specific violations.

class SafetyRule:
    def __init__(self, pattern, message, severity='ERROR', code_only=True):
        self.pattern = re.compile(pattern)
        self.message = message
        self.severity = severity
        self.code_only = code_only  # If True, pattern matches are ignored inside comments/strings

RULES = [
    # --- Memory Safety (Critical) ---
    SafetyRule(r'\bnew\s+', 'Raw "new" detected. Use std::make_unique, std::make_shared, or EnTT.', 'CRITICAL'),
    SafetyRule(r'\bdelete\s+', 'Raw "delete" detected. Use RAII/Smart Pointers.', 'CRITICAL'),
    SafetyRule(r'\bmalloc\(', 'Legacy "malloc" detected. Use mimalloc-aware containers or new.', 'CRITICAL'),
    SafetyRule(r'\bfree\(', 'Legacy "free" detected. Use RAII.', 'CRITICAL'),
    SafetyRule(r'\breinterpret_cast<', 'Unsafe "reinterpret_cast" detected. Verify pointer aliasing rules.', 'WARNING'),
    SafetyRule(r'\bconst_cast<', '"const_cast" detected. Modifying const data is UB.', 'WARNING'),
    
    # --- Legacy C APIs ---
    SafetyRule(r'\bprintf\(', 'Legacy "printf" detected. Use spdlog or fmt::print.', 'WARNING'),
    SafetyRule(r'\bsprintf\(', 'Unsafe "sprintf" detected. Use fmt::format or snprintf.', 'ERROR'),
    SafetyRule(r'\bstrcpy\(', 'Unsafe "strcpy" detected. Use std::string or strncpy.', 'ERROR'),
    SafetyRule(r'\bmemset\(', 'Low-level "memset" detected. Prefer std::fill or constructor initialization.', 'WARNING'),
    SafetyRule(r'\bmemcpy\(', 'Low-level "memcpy" detected. Ensure bounds are checked. Prefer std::copy.', 'WARNING'),

    # --- EnTT / ECS Safety ---
    SafetyRule(r'&\s*[a-zA-Z0-9_]+Component', 'Storing reference to Component detected? References to EnTT components are unstable across frame updates.', 'WARNING'),
    SafetyRule(r'\*\s*[a-zA-Z0-9_]+Component', 'Storing pointer to Component detected? Pointers to EnTT components are unstable.', 'WARNING'),
    
    # --- Concurrency ---
    SafetyRule(r'\bstatic\s+[a-zA-Z0-9_]+(\s*\*|\s+)[a-zA-Z0-9_]+\s*=', 'Function-local static variable with assignment detected. Potential thread-safety issue.', 'WARNING'),
    
    # --- Hygiene ---
    SafetyRule(r'using\s+namespace\s+std;', 'Global "using namespace std;" in header/source is discouraged. Use specific using-declarations.', 'WARNING'),
]

def mask_comments_and_strings(text):
    """
    Replaces comments and string literals with spaces.
    """
    # Use ASCII construction to avoid write_file escaping issues
    BS = chr(92)
    BS_ESC = BS + BS 
    QUOTE = '"'
    SQUOTE = "'"
    
    # Block comment: /* ... */
    # Regex: (?s:/*.*?)*/
    p_block = '(?s:/' + BS_ESC + '*.*?' + BS_ESC + '*/)'
    
    # Line comment: // ...
    p_line = '//.*'
    
    # String literal: " ... "
    # Regex: "(?:\\.|[^\\"])*"
    p_str = QUOTE + '(?:' + BS_ESC + '.|[^' + BS_ESC + QUOTE + '])*' + QUOTE
    
    # Char literal: ' ... '
    p_char = SQUOTE + '(?:' + BS_ESC + '.|[^' + BS_ESC + SQUOTE + '])*' + SQUOTE

    full_pattern = f'({p_block})|({p_line})|({p_str})|({p_char})'
    try:
        pattern = re.compile(full_pattern)
    except Exception as e:
        print(f"DEBUG: Failed to compile regex: {full_pattern}")
        raise e

    def replacer(match):
        s = match.group(0)
        if s.startswith('/*'):
            # Preserve newlines in block comments
            return ''.join([c if c == chr(10) else ' ' for c in s])
        else:
            return ' ' * len(s)

    return pattern.sub(replacer, text)

def scan_file(filepath):
    issues = []
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
            
        # Create a "clean" version for code-only checks
        try:
            clean_content = mask_comments_and_strings(content)
        except Exception as e:
             # Fallback if masking fails
             clean_content = content
             # print(f"Warning: Regex masking failed for {filepath}: {e}")

        lines = content.splitlines() # Original lines for display
        clean_lines = clean_content.splitlines() # Clean lines for matching positions
        
        # Check alignment (basic heuristic)
        if len(lines) != len(clean_lines):
            pass

        for rule in RULES:
            # We iterate over lines to report line numbers
            for i, line in enumerate(clean_lines):
                if i >= len(lines): break 
                
                check_line = line if rule.code_only else lines[i] 
                
                match = rule.pattern.search(check_line)
                if match:
                    # Context generation (3 lines)
                    start = max(0, i - 1)
                    end = min(len(lines), i + 2)
                    context_lines = lines[start:end]
                    
                    # Format issue
                    issue_str = f"[{rule.severity}] {filepath}:{i+1} - {rule.message}\n"
                    for j, ctx_line in enumerate(context_lines):
                        ln = start + j + 1
                        marker = ">>>" if ln == i + 1 else "   "
                        issue_str += f"{marker} {ln}: {ctx_line.rstrip()}\n"
                    
                    issues.append(issue_str)
                    
    except Exception as e:
        print(f"Error scanning {filepath}: {e}")
        
    return issues

def main():
    target_dir = 'src'
    if len(sys.argv) > 1:
        target_dir = sys.argv[1]
        
    print(f"Starting Enhanced Safety Scan on: {target_dir}")
    print("=" * 60)
    
    total_issues = 0
    file_count = 0
    
    for root, dirs, files in os.walk(target_dir):
        for file in files:
            if file.endswith(('.cpp', '.hpp', '.c', '.h')):
                file_count += 1
                filepath = os.path.join(root, file)
                file_issues = scan_file(filepath)
                
                if file_issues:
                    for issue in file_issues:
                        print(issue)
                    total_issues += len(file_issues)
    
    print("=" * 60)
    print(f"Scan Complete. Scanned {file_count} files.")
    
    if total_issues > 0:
        print(f"FAILURE: Found {total_issues} potential safety violations.")
        sys.exit(1)
    else:
        print("SUCCESS: No safety violations found.")
        sys.exit(0)

if __name__ == '__main__':
    main()