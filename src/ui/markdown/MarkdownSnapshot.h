#pragma once

#include "MarkdownDocument.h"
#include "MarkdownSource.h"

#include <QVector>
#include <memory>

namespace ui::markdown {

using BlockId = quint64;
using ElementId = quint64;

struct ElementKey {
    BlockId blockId = 0;
    ElementId elementId = 0;

    friend bool operator==(const ElementKey&, const ElementKey&) = default;
};

inline size_t qHash(const ElementKey& key, size_t seed = 0) noexcept
{
    return qHashMulti(seed, key.blockId, key.elementId);
}

struct ParsedBlock {
    MarkdownNodeType kind = MarkdownNodeType::Unknown;
    SourceRange sourceRange;
    quint64 sourceHash = 0;
    quint64 semanticHash = 0;
    const MarkdownNode* node = nullptr;
    bool provisional = false;
};

struct ParsedDocumentSnapshot {
    SourceRevision sourceRevision = 0;
    ProjectionRevision projectionRevision = 0;
    quint64 semanticEnvironmentRevision = 0;
    std::shared_ptr<const MarkdownDocument> document;
    QVector<ParsedBlock> blocks;
};

struct MarkdownBlock : ParsedBlock {
    BlockId id = 0;
};

struct DocumentSnapshot {
    SourceRevision sourceRevision = 0;
    ProjectionRevision projectionRevision = 0;
    quint64 semanticEnvironmentRevision = 0;
    std::shared_ptr<const MarkdownDocument> document;
    QVector<MarkdownBlock> blocks;
};

enum class BlockChangeKind { Unchanged, Updated, Inserted, Removed };

struct BlockChange {
    BlockId id = 0;
    int oldIndex = -1;
    int newIndex = -1;
    BlockChangeKind kind = BlockChangeKind::Unchanged;
};

struct BlockChangeSet {
    QVector<BlockChange> changes;
    int unchangedCount = 0;
    int updatedCount = 0;
    int insertedCount = 0;
    int removedCount = 0;
};

class MarkdownSnapshotParser final
{
public:
    ParsedDocumentSnapshot parse(const ParseProjection& projection,
                                 const MarkdownParseOptions& options = {}) const;
};

class BlockReconciler final
{
public:
    DocumentSnapshot reconcile(const ParsedDocumentSnapshot& parsed,
                               const DocumentSnapshot* previous,
                               BlockChangeSet* changes = nullptr);

private:
    BlockId m_nextId = 1;
};

} // namespace ui::markdown
