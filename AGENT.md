# iiGUI 작업 지침

C++23과 표준·iiSDK의 공개 인터페이스를 사용한다. SDK 소스와 헤더는 src/에서 함께 유지한다. 새 네이티브 경로에 Qt 의존성을 추가하지 않는다.

iiGUI는 C++ 그래픽 인터페이스 프레임워크이며 시각 컴포넌트의 상태·동작·배치·
입력·접근성과 그리기를 소유하고 GPU 장면으로 표시한다. LVUI는 이를 선언하는
별도 언어이며 iiGUI는 LVUI에 의존하지 않는다. [프레임워크 경계](docs/FrameworkBoundary.md)를 따른다.
데스크톱 앱에 브라우저나 WebView를 내장하거나 컴포넌트마다 OS 위젯을 생성하지
않는다. 창·표면·입력·접근성·글꼴 등은 OS 서비스로 연결한다. 현재 CPU 래스터화와
하드웨어 선택, 레거시 경로의 한계는 [GPU 장면 합성](docs/GpuSceneComposition.md)을 따른다.

빌드 산출물은 저장소의 build/에 둔다. 테스트용 임시 데이터와 외부 소스는 resources/ 아래에서 관리하며 테스트 임시 데이터는 검증 후 지운다. 소스 변경 시 관련 docs/와 tests/를 함께 갱신한다. README.md는 영어, docs/ 문서는 한국어로 작성한다. 기존 작업과 사용자 파일을 보존한다. 플랫폼별 소스 이식과 실제 실행 검증을 구분한다.

<!-- workspace-docs:agent:start -->
## 문서 구조와 에이전트 탐색

- 프로젝트 루트의 `README.md`는 사람용 영문 진입 문서이며 `AGENT.md`는 에이전트 진입점이다.
- 하위 인간용 Markdown과 구조화된 agent JSON을 함께 유지한다. Markdown이 원본이며 JSON은 원문 해시와 줄 범위를 가진 파생 문서이다.
- 먼저 [docs/agent-index.json](docs/agent-index.json)에서 관련 문서와 JSON 투영을 찾는다. 원문 SHA-256이 다르면 현재 Markdown을 읽고 투영을 갱신한다.
- JSON의 규칙 후보는 검색용 어휘 일치 결과이다. 과거 기록·예제·실패 상태를 현재 지시나 검증 완료로 자동 해석하지 않는다.
- 기존 프로젝트별 규칙, 동결된 증거, 라이선스, 외부 원본, 사용자 데이터와 다른 작업의 변경을 보존한다.
- 이 문서 재편 지침은 과거 작업의 기능 구현·배포를 자동 재개하라는 요청이 아니다. 현재 사용자의 작업 범위를 우선한다.
- 문서를 수정하면 JSON 쌍·색인을 함께 갱신하고 검증한다. 기존 소스 변경에 대한 빌드·테스트 규칙은 계속 따른다.

Workspace에서 이 프로젝트의 문서를 갱신하는 명령은 다음과 같다.

```sh
python3 ../../Engineering/WorkspaceDocumentation/App/workspace_docs.py --workspace ../.. sync --project SDK/iiGUI
```
<!-- workspace-docs:agent:end -->
