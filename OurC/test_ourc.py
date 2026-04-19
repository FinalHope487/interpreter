import asyncio
import json
import sys
import os

# 注意：此腳本需要安裝 websockets 函式庫
# 安裝指令: pip install websockets

try:
    import websockets
except ImportError:
    print("請先安裝 websockets 函式庫: pip install websockets")
    sys.exit(1)

class OurCClient:
    """
    OurC 網站的爬蟲程式類別，直接透過 WebSocket 與後端通訊。
    網站網址: https://cycu-ice-pl.github.io/website/#/OurC
    """
    def __init__(self, interpreter_type="OurCproject1"):
        # 經分析網站源碼與網路封包所得的 WebSocket 網址
        self.uri = "wss://pl-tmp.ja-errorpro.codes/"
        self.interpreter_type = interpreter_type

    async def run_code(self, code):
        """
        傳送程式碼並接收輸出結果
        """
        try:
            async with websockets.connect(self.uri) as websocket:
                # 構造傳送給伺服器的 JSON 資料
                message = {
                    "interpreterType": self.interpreter_type,
                    "payload": code if code.endswith("\n") else code + "\n"
                }
                
                # 傳送資料
                await websocket.send(json.dumps(message))
                
                output_parts = []
                # 持續接收直到伺服器回傳 "ready" 信號
                while True:
                    try:
                        response = await websocket.recv()
                    except websockets.ConnectionClosed:
                        break
                    
                    try:
                        # 嘗試解析為 JSON
                        data = json.loads(response)
                        
                        # 略過確認訊息 (ack)
                        if data.get("type") == "ack":
                            continue
                        
                        # 收到完成信號時退出
                        if data.get("type") == "ready":
                            break
                            
                    except json.JSONDecodeError:
                        # 如果不是 JSON，則為編譯器/直譯器的原始輸出文字
                        output_parts.append(response)
                
                return "".join(output_parts)
                
        except Exception as e:
            return f"連線或執行發生錯誤: {str(e)}"

async def main():
    # 預設專案類型
    project_type = "OurCproject1"
    
    # 範例程式碼 (根據 OurC 語法)
    # 註：有些版本可能需要先輸入一個數字代表專案類型，例如 "1\n..."
    test_code = "1\nint main() { cout << \"Hello from Crawler!\" << endl ; }"
    
    if len(sys.argv) > 1:
        target = sys.argv[1]
        if os.path.exists(target):
            with open(target, 'r', encoding='utf-8') as f:
                test_code = f.read()
            print(f"--- 正在從檔案讀取程式碼: {target} ---")
        else:
            test_code = target
            print(f"--- 正在使用命令列輸入的程式碼 ---")
    else:
        print(f"--- 正在使用預設測試程式碼 ({project_type}) ---")

    client = OurCClient(interpreter_type=project_type)
    
    print("-" * 50)
    print(test_code)
    print("-" * 50)
    
    result = await client.run_code(test_code)
    
    print("--- 接收到的輸出結果 ---")
    if not result.strip():
        print("(無輸出)")
    else:
        print(result)
    print("-" * 50)

if __name__ == "__main__":
    # 執行非同步主程式
    asyncio.run(main())
