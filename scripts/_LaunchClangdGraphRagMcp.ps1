$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$ragRoot = "F:\devtools\clangd-graph-rag"

$apiKey = [Environment]::GetEnvironmentVariable("SI_API_KEY", "Process")
if ([string]::IsNullOrWhiteSpace($apiKey)) {
    $apiKey = [Environment]::GetEnvironmentVariable("SI_API_KEY", "User")
}
if ([string]::IsNullOrWhiteSpace($apiKey)) {
    throw "Missing SI_API_KEY user environment variable."
}

$neo4jPassword = [Environment]::GetEnvironmentVariable("NEO4J_PASSWORD", "Process")
if ([string]::IsNullOrWhiteSpace($neo4jPassword)) {
    $neo4jPassword = [Environment]::GetEnvironmentVariable("NEO4J_PASSWORD", "User")
}
if ([string]::IsNullOrWhiteSpace($neo4jPassword)) {
    throw "Missing NEO4J_PASSWORD user environment variable."
}

$env:OPENAI_API_KEY = $apiKey
$env:OPENAI_BASE_URL = "https://api.siliconflow.cn/v1"
$env:OPENAI_MODEL = "Qwen/Qwen3-8B"
$env:OPENAI_EMBEDDING_MODEL = "Qwen/Qwen3-Embedding-0.6B"
$env:EMBEDDING_API = "openai"

$env:NEO4J_URI = "bolt://localhost:7687"
$env:NEO4J_USER = "neo4j"
$env:NEO4J_PASSWORD = $neo4jPassword
$env:PROJECT_ROOT_PATH = $repoRoot

$env:MCP_TRANSPORT = "stdio"

$mcpServer = Join-Path $ragRoot "graph_mcp_server.py"
if (-not (Test-Path $mcpServer)) {
    throw "Missing graph_mcp_server.py at $mcpServer"
}

& conda run --no-capture-output -n ai python $mcpServer
