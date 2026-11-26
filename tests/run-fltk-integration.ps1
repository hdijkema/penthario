param(
    [string]$PostgreSqlBin = (Join-Path $PSScriptRoot '..\third_party\postgresql18\pgsql\bin'),
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\build\fltk')
)
$ErrorActionPreference = 'Stop'
$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
$fixture = Join-Path $BuildDirectory ('integration-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
$data = Join-Path $fixture 'data'
$listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
$listener.Start()
$port = $listener.LocalEndpoint.Port
$listener.Stop()
$started = $false
$oldPort = $env:PGADM_TEST_PORT
$oldData = $env:PGADM_TEST_DATA
$oldStall = $env:PGADM_TEST_STALL_PORT
$stallListener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
$stallListener.Start()
$pgEnvironment = @{}
foreach ($key in @('PGSERVICE','PGHOSTADDR','PGPASSWORD','PGPASSFILE','PGOPTIONS')) {
    $pgEnvironment[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
    [Environment]::SetEnvironmentVariable($key, '', 'Process')
}
$passfile = Join-Path $fixture 'empty.pgpass'
[System.IO.File]::WriteAllText($passfile, '')
$env:PGPASSFILE = $passfile
try {
    & (Join-Path $PostgreSqlBin 'initdb.exe') -D $data -U postgres --auth=trust --encoding=UTF8 --no-locale
    if ($LASTEXITCODE -ne 0) { throw 'initdb failed' }
    & (Join-Path $PostgreSqlBin 'pg_ctl.exe') -D $data -l (Join-Path $fixture 'postgres.log') -o "-p $port -h 127.0.0.1" -w -t 20 start
    if ($LASTEXITCODE -ne 0) { throw 'Temporary PostgreSQL could not start' }
    $started = $true
    $env:PGADM_TEST_PORT = [string]$port
    $env:PGADM_TEST_DATA = $data
    $env:PGADM_TEST_STALL_PORT = [string]$stallListener.LocalEndpoint.Port
    & (Join-Path $BuildDirectory 'penthario-tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'FLTK integration tests failed' }
    & (Join-Path $BuildDirectory 'penthario.exe') --settings-file (Join-Path $fixture 'workspace.json') --startup-check
    if ($LASTEXITCODE -ne 0) { throw 'FLTK startup check failed' }
} finally {
    $env:PGADM_TEST_PORT = $oldPort
    $env:PGADM_TEST_DATA = $oldData
    $env:PGADM_TEST_STALL_PORT = $oldStall
    $stallListener.Stop()
    foreach ($key in $pgEnvironment.Keys) { [Environment]::SetEnvironmentVariable($key, $pgEnvironment[$key], 'Process') }
    if ($started) {
        & (Join-Path $PostgreSqlBin 'pg_ctl.exe') -D $data -m fast -w -t 20 stop
        if ($LASTEXITCODE -ne 0) { Write-Warning "Temporary server shutdown failed: $data" }
    }
    Write-Output "Isolated test fixture: $fixture"
}
