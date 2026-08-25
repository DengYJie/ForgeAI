#pragma once

#include "domain/mcp/McpServerConfig.h"
#include "domain/mcp/McpTransportType.h"
#include "domain/mcp/McpConnectionState.h"
#include "domain/mcp/McpError.h"
#include "domain/mcp/McpResource.h"
#include "domain/mcp/McpPrompt.h"

namespace llm::mcp {
    using McpServerConfig = domain::mcp::McpServerConfig;
    using McpTransportType = domain::mcp::McpTransportType;
    using McpConnectionState = domain::mcp::McpConnectionState;
    using McpError = domain::mcp::McpError;
    using McpErrorCode = domain::mcp::McpErrorCode;
    using McpResource = domain::mcp::McpResource;
    using McpPrompt = domain::mcp::McpPrompt;
}
