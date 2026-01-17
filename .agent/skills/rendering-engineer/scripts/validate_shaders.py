import os
import sys
import subprocess
import shutil

SHADER_EXTS = ['.vert', '.frag', '.compute', '.geom', '.tesc', '.tese', '.fs', '.vs']

def main():
    print("Looking for glslangValidator...")
    validator = shutil.which('glslangValidator')
    
    if not validator:
        print("ERROR: glslangValidator not found in PATH.")
        print("Please download it from the SDK or Khronos GitHub release.")
        print("https://github.com/KhronosGroup/glslang/releases")
        # Exit with 0 to not break the build strictly if user doesn't have it, but warn.
        # Or exit 1 if we want to enforce it. Let's warn for now.
        print("Skipping shader validation.")
        sys.exit(0)
    
    print(f"Found validator: {validator}")
    
    shader_dir = 'assets/shaders'
    if len(sys.argv) > 1:
        shader_dir = sys.argv[1]
        
    print(f"Scanning shaders in {shader_dir}...")
    
    failed_shaders = []
    
    for root, dirs, files in os.walk(shader_dir):
        for file in files:
            ext = os.path.splitext(file)[1]
            if ext in SHADER_EXTS:
                path = os.path.join(root, file)
                print(f"Validating {path}...")
                
                # glslangValidator usage: glslangValidator [option]... [file]...
                # -l link ? No, just compile check per file usually.
                try:
                    # Capture output
                    result = subprocess.run(
                        [validator, path], 
                        capture_output=True, 
                        text=True
                    )
                    
                    if result.returncode != 0:
                        print(f"FAILED: {path}")
                        print(result.stdout)
                        print(result.stderr)
                        failed_shaders.append(path)
                    else:
                        # Only verbose if needed
                        pass
                except Exception as e:
                    print(f"Error executing validator on {path}: {e}")
                    failed_shaders.append(path)

    if failed_shaders:
        print(f"\n{len(failed_shaders)} shaders failed validation.")
        sys.exit(1)
    else:
        print("\nAll shaders passed validation.")
        sys.exit(0)

if __name__ == '__main__':
    main()
