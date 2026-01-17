import os

TRACKS_DIR = 'conductor/tracks'

def main():
    if not os.path.exists(TRACKS_DIR):
        print("Tracks directory not found.")
        return

    print("Project Tracks Structure:")
    for root, dirs, files in os.walk(TRACKS_DIR):
        level = root.replace(TRACKS_DIR, '').count(os.sep)
        indent = ' ' * 4 * (level)
        print(f'{indent}{os.path.basename(root)}/')
        subindent = ' ' * 4 * (level + 1)
        for f in files:
            if f.endswith('.md'):
                print(f'{subindent}{f}')

if __name__ == '__main__':
    main()
