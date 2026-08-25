target_sources(ForgeAICore PRIVATE
    sqlite/SqlTransaction.h
    sqlite/SqlTransaction.cpp
    sqlite/SqlHelper.h
    sqlite/SqlHelper.cpp
    sqlite/DatabaseManager.h
    sqlite/DatabaseManager.cpp
    repository/SqliteConversationRepository.h
    repository/SqliteConversationRepository.cpp
    repository/SqliteProjectRepository.h
    repository/SqliteProjectRepository.cpp
    repository/SqliteModelRepository.h
    repository/SqliteModelRepository.cpp
    repository/JsonlMessageRepository.h
    repository/JsonlMessageRepository.cpp
    importer/ModelsDevImporter.h
    importer/ModelsDevImporter.cpp
)