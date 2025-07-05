# danttoUHD
**danttoUHD**는 한국 지상파 UHD 방송(ATSC 3.0)를 MPEG-2 TS로 변환하는 툴입니다.
현재 ROUTE 방식만 지원하며, MMT 방식(KBS1 등)은 지원되지 않습니다.

## 사용 방법
```
./danttoUHD.exe <input> <output.ts>
options:
	--casServerUrl=<url>
```
LG 지상파 UHD 셋탑박스 AN-US800K의 자체 컨테이너 형식만 지원하며, 다른 포맷은 지원하지 않습니다.

## CAS
이 프로젝트는 복호화 기능이 포함되어 있지만, 복호화 키를 추출하는 기능은 포함되어 있지 않습니다.
따라서 암호화되지 않은 지역 방송(TJB, KNN 등)에만 사용할 수 있습니다.

## TODO
- MMT 지원
- EPG 변환
