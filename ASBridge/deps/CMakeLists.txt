include(FetchContent)

FetchContent_Declare(
    Validate
    GIT_REPOSITORY https://github.com/Unchpokable/Validate-.git
    GIT_TAG        main  # or a specific tag/commit
)

FetchContent_MakeAvailable(Validate)

target_link_libraries(asbridge PRIVATE Validate::vd)
