import json
from huggingface_hub import HfApi
from huggingface_hub.utils import RepositoryNotFoundError

def verify_models():
    api = HfApi()
    
    # Load your models.json file
    try:
        with open('models.json', 'r') as f:
            models = json.load(f)
    except FileNotFoundError:
        print("[Error] models.json not found in this directory.")
        return

    print(f"Verifying {len(models)} models on Hugging Face...\n")
    
    all_valid = True
    
    for m in models:
        repo_id = m.get('repo_id')
        alias = m.get('alias')
        
        try:
            # Query the repo metadata without downloading the model
            info = api.repo_info(repo_id=repo_id)
            
            # Check if openvino_model.xml actually exists inside the repo
            filenames = [f.rfilename for f in info.siblings]
            if "openvino_model.xml" in filenames:
                print(f"[✅ VALID] {alias.ljust(25)} -> {repo_id}")
            else:
                print(f"[❌ NO XML] {alias.ljust(25)} -> {repo_id} (Repo exists, but lacks OpenVINO XML)")
                all_valid = False
                
        except RepositoryNotFoundError:
            print(f"[❌ 401/404] {alias.ljust(24)} -> {repo_id} (Repository does not exist or is private)")
            all_valid = False
            
        except Exception as e:
            print(f"[⚠️ ERROR] {alias.ljust(25)} -> {repo_id} ({str(e)})")
            all_valid = False
            
    print("\n" + "="*50)
    if all_valid:
        print("🎉 SUCCESS: All models are fully valid and ready to download!")
    else:
        print("⚠️ WARNING: Some models failed verification. Please update models.json.")
        
if __name__ == "__main__":
    verify_models()