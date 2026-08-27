#include "MarkdownSnapshot.h"

#include <QHash>
#include <QRegularExpression>

namespace ui::markdown {
namespace {

quint64 hashBytes(quint64 hash, const void* data, qsizetype size)
{
    constexpr quint64 prime = 1099511628211ULL;
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (qsizetype i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= prime;
    }
    return hash;
}

quint64 hashString(QStringView text, quint64 seed = 1469598103934665603ULL)
{
    return hashBytes(seed, text.data(), text.size() * static_cast<qsizetype>(sizeof(QChar)));
}

quint64 semanticHash(const MarkdownNode& node, quint64 hash = 1469598103934665603ULL)
{
    const auto type = static_cast<quint32>(node.type);
    hash = hashBytes(hash, &type, sizeof(type));
    hash = hashString(node.literal, hash);
    hash = hashString(node.attributes.url, hash);
    hash = hashString(node.attributes.title, hash);
    hash = hashString(node.attributes.fenceInfo, hash);
    const quint64 attributes = static_cast<quint64>(node.attributes.headingLevel)
        | (static_cast<quint64>(node.attributes.listStart) << 8)
        | (static_cast<quint64>(node.attributes.orderedList) << 40)
        | (static_cast<quint64>(node.attributes.taskListItem) << 41)
        | (static_cast<quint64>(node.attributes.taskChecked) << 42)
        | (static_cast<quint64>(node.attributes.tableHeader) << 43);
    hash = hashBytes(hash, &attributes, sizeof(attributes));
    for (const auto& child : node.children)
        hash = semanticHash(*child, hash);
    return hash;
}

bool overlaps(const SourceRange& a, const SourceRange& b)
{
    return a.begin < b.end && b.begin < a.end;
}

int matchScore(const MarkdownBlock& oldBlock, const ParsedBlock& newBlock, int oldIndex, int newIndex)
{
    int score = 0;
    if (oldBlock.sourceHash == newBlock.sourceHash) score += 1000;
    if (oldBlock.semanticHash == newBlock.semanticHash) score += 500;
    if (overlaps(oldBlock.sourceRange, newBlock.sourceRange)) score += 250;
    if (oldBlock.kind == newBlock.kind) score += 100;
    score -= qAbs(oldIndex - newIndex) * 3;
    return score;
}

} // namespace

ParsedDocumentSnapshot MarkdownSnapshotParser::parse(const ParseProjection& projection,
                                                      const MarkdownParseOptions& options) const
{
    ParsedDocumentSnapshot result;
    result.sourceRevision = projection.sourceRevision;
    result.projectionRevision = projection.projectionRevision;
    auto document = std::make_shared<MarkdownDocument>(MarkdownParser{}.parse(projection.text, options));
    result.document = document;

    const QStringView source{projection.text};
    for (const auto& node : document->root().children) {
        ParsedBlock block;
        block.kind = node->type;
        block.sourceRange = node->sourceRange;
        block.node = node.get();
        const qsizetype begin = qBound<qsizetype>(0, block.sourceRange.begin, source.size());
        const qsizetype end = qBound<qsizetype>(begin, block.sourceRange.end, source.size());
        block.sourceHash = hashString(source.mid(begin, end - begin));
        block.semanticHash = semanticHash(*node);
        result.blocks.push_back(block);
    }

    static const QRegularExpression definition(
        QStringLiteral("(?m)^[ \\t]{0,3}\\[[^\\]\\r\\n]+\\]:[ \\t]*.*$"));
    quint64 environment = 1469598103934665603ULL;
    auto it = definition.globalMatch(projection.text);
    while (it.hasNext())
        environment = hashString(it.next().capturedView(), environment);
    result.semanticEnvironmentRevision = environment;
    return result;
}

DocumentSnapshot BlockReconciler::reconcile(const ParsedDocumentSnapshot& parsed,
                                            const DocumentSnapshot* previous,
                                            BlockChangeSet* changeSet)
{
    BlockChangeSet localChanges;
    DocumentSnapshot result;
    result.sourceRevision = parsed.sourceRevision;
    result.projectionRevision = parsed.projectionRevision;
    result.semanticEnvironmentRevision = parsed.semanticEnvironmentRevision;
    result.document = parsed.document;
    result.blocks.resize(parsed.blocks.size());

    QVector<int> oldForNew(parsed.blocks.size(), -1);
    QVector<bool> oldUsed(previous ? previous->blocks.size() : 0, false);
    int prefix = 0;
    if (previous) {
        const int common = qMin(previous->blocks.size(), parsed.blocks.size());
        while (prefix < common
               && previous->blocks[prefix].sourceHash == parsed.blocks[prefix].sourceHash
               && previous->blocks[prefix].kind == parsed.blocks[prefix].kind) {
            oldForNew[prefix] = prefix;
            oldUsed[prefix] = true;
            ++prefix;
        }

        int oldSuffix = previous->blocks.size() - 1;
        int newSuffix = parsed.blocks.size() - 1;
        while (oldSuffix >= prefix && newSuffix >= prefix
               && previous->blocks[oldSuffix].sourceHash == parsed.blocks[newSuffix].sourceHash
               && previous->blocks[oldSuffix].kind == parsed.blocks[newSuffix].kind) {
            oldForNew[newSuffix] = oldSuffix;
            oldUsed[oldSuffix] = true;
            --oldSuffix;
            --newSuffix;
        }

        for (int newIndex = prefix; newIndex <= newSuffix; ++newIndex) {
            int bestOld = -1;
            int bestScore = 0;
            const int firstOld = qMax(prefix, newIndex - 32);
            const int lastOld = qMin(oldSuffix, newIndex + 32);
            for (int oldIndex = firstOld; oldIndex <= lastOld; ++oldIndex) {
                if (oldUsed[oldIndex]) continue;
                const int score = matchScore(previous->blocks[oldIndex], parsed.blocks[newIndex], oldIndex, newIndex);
                if (score > bestScore) {
                    bestScore = score;
                    bestOld = oldIndex;
                }
            }
            if (bestOld >= 0 && bestScore >= 200) {
                oldForNew[newIndex] = bestOld;
                oldUsed[bestOld] = true;
            }
        }
    }

    for (int newIndex = 0; newIndex < parsed.blocks.size(); ++newIndex) {
        const ParsedBlock& parsedBlock = parsed.blocks[newIndex];
        MarkdownBlock block;
        static_cast<ParsedBlock&>(block) = parsedBlock;
        const int oldIndex = oldForNew[newIndex];
        BlockChange change;
        change.newIndex = newIndex;
        if (previous && oldIndex >= 0) {
            block.id = previous->blocks[oldIndex].id;
            change.id = block.id;
            change.oldIndex = oldIndex;
            const auto& old = previous->blocks[oldIndex];
            change.kind = old.semanticHash == block.semanticHash
                    && old.sourceHash == block.sourceHash
                    && previous->semanticEnvironmentRevision == result.semanticEnvironmentRevision
                ? BlockChangeKind::Unchanged : BlockChangeKind::Updated;
        } else {
            block.id = m_nextId++;
            change.id = block.id;
            change.kind = BlockChangeKind::Inserted;
        }
        result.blocks[newIndex] = block;
        localChanges.changes.push_back(change);
    }

    if (previous) {
        for (int oldIndex = 0; oldIndex < oldUsed.size(); ++oldIndex) {
            if (!oldUsed[oldIndex])
                localChanges.changes.push_back({previous->blocks[oldIndex].id, oldIndex, -1, BlockChangeKind::Removed});
        }
    }
    for (const auto& change : localChanges.changes) {
        switch (change.kind) {
        case BlockChangeKind::Unchanged: ++localChanges.unchangedCount; break;
        case BlockChangeKind::Updated: ++localChanges.updatedCount; break;
        case BlockChangeKind::Inserted: ++localChanges.insertedCount; break;
        case BlockChangeKind::Removed: ++localChanges.removedCount; break;
        }
    }
    if (changeSet) *changeSet = localChanges;
    return result;
}

} // namespace ui::markdown
