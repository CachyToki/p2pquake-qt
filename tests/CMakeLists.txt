qt_add_executable(tst_p2pquake tst_p2pquake.cpp)
target_link_libraries(tst_p2pquake PRIVATE p2pquake Qt6::Test)
target_compile_definitions(tst_p2pquake PRIVATE
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME tst_p2pquake COMMAND tst_p2pquake)
set_tests_properties(tst_p2pquake PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# 開発用: メインウィンドウを画面なしで描画して PNG に保存する。
#   QT_QPA_PLATFORM=offscreen ./tests/render_window out.png              (フィクスチャを表示)
#   QT_QPA_PLATFORM=offscreen ./tests/render_window out.png --live       (本番の API に接続)
#   QT_QPA_PLATFORM=offscreen ./tests/render_window out.png --sandbox --wait 45
qt_add_executable(render_window render_window.cpp)
target_link_libraries(render_window PRIVATE p2pquake)
target_compile_definitions(render_window PRIVATE
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
