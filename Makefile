# ============================================================================
# Reharm Studio — ビルド/テストの正式な入口
#
# ポリシー: 正規のビルドディレクトリは build/ のみ。
#   - リリース前の最終テストは必ず build/ で実施する → `make release-check`
#   - 並行作業するエージェントは専用サンドボックス（.agent-builds/<name>）を
#     使う → `make agent-test AGENT=codex` など。サンドボックスのテスト出力には
#     自動で [AGENT SANDBOX BUILD] の印が付き、リリース検証としては無効。
# ============================================================================

.DEFAULT_GOAL := help
.PHONY: help app test release-check agent-test clean-agents

CANONICAL_TESTS := build/ReharmStudioTests_artefacts/Debug/ReharmStudioTests

help:
	@echo "Reharm Studio — 使い方:"
	@echo ""
	@echo "  make app                    アプリ本体をビルド (build/, Debug)"
	@echo "  make test                   テストを build/ でビルド・実行（正規）"
	@echo "  make release-check          リリース前の最終テスト（build/ で実施。これが唯一の正式検証）"
	@echo "  make agent-test AGENT=codex エージェント用サンドボックスでテスト (.agent-builds/codex)"
	@echo "  make agent-test AGENT=grok  同上 (.agent-builds/grok)"
	@echo "  make clean-agents           サンドボックスを全削除"
	@echo ""
	@echo "  ※ build/ 以外で実行したテストの出力には [AGENT SANDBOX BUILD] の印が付き、"
	@echo "     リリース検証としてカウントされません。"

app:
	cmake --preset default
	cmake --build --preset default --parallel

test:
	cmake --preset debug-tests
	cmake --build --preset debug-tests --target ReharmStudioTests --parallel
	./$(CANONICAL_TESTS)

release-check: test
	@echo ""
	@echo "release-check OK — build/ (正規ビルドディレクトリ) で全テスト通過。"

agent-test:
	@test -n "$(AGENT)" || { echo "使い方: make agent-test AGENT=codex|grok"; exit 1; }
	cmake --preset agent-$(AGENT)
	cmake --build --preset agent-$(AGENT) --target ReharmStudioTests --parallel
	./.agent-builds/$(AGENT)/ReharmStudioTests_artefacts/Debug/ReharmStudioTests

clean-agents:
	rm -rf .agent-builds
