include("F:/items/tuopuyouhua/GraphVisualizer/build/Desktop_Qt_6_11_1_MinGW_64_bit-Profile/.qt/QtDeploySupport.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/GraphVisualizer-plugins.cmake" OPTIONAL)
set(__QT_DEPLOY_I18N_CATALOGS "qtbase")

qt6_deploy_runtime_dependencies(
    EXECUTABLE "F:/items/tuopuyouhua/GraphVisualizer/build/Desktop_Qt_6_11_1_MinGW_64_bit-Profile/GraphVisualizer.exe"
    GENERATE_QT_CONF
)
