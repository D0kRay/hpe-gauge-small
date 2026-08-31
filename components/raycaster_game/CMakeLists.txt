idf_component_register(
    SRCS
        "raycaster_game.cpp"
    INCLUDE_DIRS "." "include"
    PRIV_INCLUDE_DIRS "."
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
