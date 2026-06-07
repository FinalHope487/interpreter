import os
import subprocess
import glob

def run_tests():
    # 使用當前腳本所在的目錄作為基準
    base_dir = os.path.dirname(os.path.abspath(__file__))
    input_dir = os.path.join(base_dir, 'inputs')
    output_dir = os.path.join(base_dir, 'outputs')
    log_dir = os.path.join(base_dir, 'error_logs')
    exe_path = os.path.join(base_dir, 'p3.exe')
    cpp_path = os.path.join(base_dir, 'p3.cpp')

    # 編譯專案
    print(f"正在編譯 {os.path.basename(cpp_path)}...")
    compile_result = subprocess.run(
        ['g++', cpp_path, '-o', exe_path],
        capture_output=True,
        text=True
    )
    
    if compile_result.returncode != 0:
        print("編譯失敗！")
        print(compile_result.stderr)
        return
    
    print("編譯成功，開始執行測試...\n")

    # 清除舊的 log 並建立 log 資料夾
    if os.path.exists(log_dir):
        for f in os.listdir(log_dir):
            os.remove(os.path.join(log_dir, f))
    else:
        os.makedirs(log_dir)

    error_log_path = os.path.join(log_dir, 'errors.log')
    error_count = 0

    # 取得所有測資檔案
    input_files = glob.glob(os.path.join(input_dir, 'p3-*.txt'))
    input_files.sort()

    if not input_files:
        print(f"找不到測資檔案於: {input_dir}")
        return

    with open(error_log_path, 'w', encoding='utf-8') as f_log:
        for input_file in input_files:
            test_filename = os.path.basename(input_file)
            test_id = os.path.splitext(test_filename)[0]
            expected_output_path = os.path.join(output_dir, test_filename)
            
            if not os.path.exists(expected_output_path):
                print(f"跳過 {test_filename}: 找不到對應的預期輸出檔案")
                continue

            try:
                # 執行 p3.exe，將測資檔案內容導向 stdin
                with open(input_file, 'r', encoding='utf-8', errors='ignore') as f_in:
                    process = subprocess.run(
                        [exe_path],
                        stdin=f_in,
                        capture_output=True,
                        text=True,
                        encoding='utf-8',
                        errors='replace'
                    )
                
                # 標準化輸出（移除頭尾空白並統一換行符號）
                my_output = process.stdout.strip().replace('\r\n', '\n')
                
                with open(expected_output_path, 'r', encoding='utf-8', errors='ignore') as f_out:
                    expected_output = f_out.read().strip().replace('\r\n', '\n')

                if my_output == expected_output:
                    print(f"{test_id} : 正確")
                else:
                    print(f"{test_id} : 有誤")
                    error_count += 1
                    # 寫入錯誤日誌
                    f_log.write(f"{test_id} :\n")
                    f_log.write("預期輸出:\n")
                    f_log.write(f"{expected_output}\n")
                    f_log.write("我的輸出:\n")
                    f_log.write(f"{my_output}\n")
                    f_log.write("\n" + "="*50 + "\n\n")

            except Exception as e:
                print(f"執行 {test_id} 時發生錯誤: {e}")
    
    if error_count == 0:
        if os.path.exists(error_log_path):
            os.remove(error_log_path)
        print("\n所有測資皆正確！")
    else:
        print(f"\n測試完成，共有 {error_count} 個錯誤。詳細資訊請見: {error_log_path}")

if __name__ == "__main__":
    run_tests()
