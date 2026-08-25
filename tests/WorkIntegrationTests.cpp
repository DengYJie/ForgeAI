#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "ui/screen/work/WorkViewModel.h"
#include "data/repository/SqliteProjectRepository.h"
#include "data/repository/SqliteConversationRepository.h"
#include "data/repository/JsonlMessageRepository.h"
#include "services/conversation/ConversationService.h"
#include "services/agent/AgentToolService.h"
#include "services/project/ProjectContextService.h"

// Note: This is a placeholder test file for the integration scenarios.
// Currently the required domain mocks (ChatGateway, ModelService, UseCases) 
// are tightly coupled with network/GUI logic and may need specialized mock classes
// before this test suite can be run with CMake / CTest.

class WorkIntegrationTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QVERIFY(m_tempDir.isValid());
    }
    
    void testProjectIsolationAndToolRoots() {
        // [P0] Validates that when switching projects, the m_workAgentToolService root updates correctly
        // and does NOT bleed into m_normalAgentToolService
    }
    
    void testWorkSessionNotVisibleInNormalChat() {
        // [P2] Validates that ordinary chat excludes work sessions
    }
    
    void testProjectDeletionCascades() {
        // [P1] Validates that WorkViewModel::removeProject deletes project and cascade-deletes the Conversation entities
    }
    
    void testSessionPinnedArchivedPersistence() {
        // [P1] Validates setSessionPinned/Archived uses ConversationService and saves to DB
    }
    
    void testTaskCancellationOnProjectSwitch() {
        // [P2] Validates that removing/archiving a project while a task is running calls cancelTask()
    }

private:
    QTemporaryDir m_tempDir;
};

QTEST_APPLESS_MAIN(WorkIntegrationTests)
#include "WorkIntegrationTests.moc"
