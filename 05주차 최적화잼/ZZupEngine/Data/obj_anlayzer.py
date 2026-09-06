import os
from collections import Counter

def analyze_obj_file(file_name):
    # 파일 존재 여부 확인
    if not os.path.exists(file_name):
        print(f"에러: '{file_name}' 파일을 찾을 수 없습니다.")
        return

    vertices = []
    
    try:
        with open(file_name, 'r', encoding='utf-8') as f:
            for line in f:
                # 줄 앞뒤 공백 제거 후 정점(v) 데이터인지 확인
                line = line.strip()
                if line.startswith('v '):
                    parts = line.split()
                    # 'v x y z' 형식이 맞는지 확인 (가끔 v에 4개 이상의 값이 올 수도 있음)
                    if len(parts) >= 4:
                        # 좌표 문자열을 튜플로 저장 (정밀도 유지를 위해 문자열 그대로 비교)
                        coords = tuple(parts[1:4])
                        vertices.append(coords)
    except Exception as e:
        print(f"파일을 읽는 중 오류가 발생했습니다: {e}")
        return

    total_v = len(vertices)
    counts = Counter(vertices)
    unique_v = len(counts)
    
    # 중복 분석
    duplicates = {pos: count for pos, count in counts.items() if count > 1}
    redundant_count = total_v - unique_v

    print("-" * 45)
    print(f" 분석 파일: {file_name}")
    print("-" * 45)
    print(f"1. 전체 정점 개수: {total_v:10,d}개")
    print(f"2. 고유 위치 개수: {unique_v:10,d}개")
    print(f"3. 제거 가능한 중복: {redundant_count:10,d}개")
    print("-" * 45)

    if duplicates:
        print(f" [상세] 총 {len(duplicates)}개의 지점에서 중복이 발생했습니다.")
        # 가장 많이 겹친 상위 3개 지점 예시
        sorted_dups = sorted(duplicates.items(), key=lambda x: x[1], reverse=True)
        for i, (pos, count) in enumerate(sorted_dups[:3]):
            print(f"   - 중복지점 {i+1}: {pos} ({count}개 겹침)")
    else:
        print(" 결과: 위치가 일치하는 정점이 없습니다.")
    print("-" * 45)

# --- 실행부 ---
filename = "apple_mid.obj"
analyze_obj_file(filename)