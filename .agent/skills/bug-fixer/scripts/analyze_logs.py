import os
import sys

LOG_FILE = 'logs/NoMoreDay.log'
ERROR_KEYWORDS = ['[error]', '[critical]', 'exception', 'assertion failed', 'crash']

def main():
    if not os.path.exists(LOG_FILE):
        print(f"Log file not found: {LOG_FILE}")
        return

    print(f"Analyzing {LOG_FILE}...")
    
    errors = []
    try:
        with open(LOG_FILE, 'r', encoding='utf-8', errors='replace') as f:
            for line in f:
                lower_line = line.lower()
                if any(k in lower_line for k in ERROR_KEYWORDS):
                    errors.append(line.strip())
    except Exception as e:
        print(f"Error reading log: {e}")
        return

    if errors:
        print(f"Found {len(errors)} error(s):")
        # Print last 20 errors
        for err in errors[-20:]:
            print(err)
    else:
        print("No obvious errors found in log.")

if __name__ == '__main__':
    main()
