param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [string]$Target = "main",

    [switch]$GenerateNotes
)

$ErrorActionPreference = "Stop"

if ($Version -notmatch '^v\d+\.\d+\.\d+$') {
    throw "Version muss im Format vX.Y.Z sein (z. B. v0.3.20)."
}

$templatePath = ".github/release-notes-template.md"

if ($GenerateNotes) {
    gh release create $Version --target $Target --title $Version --generate-notes
}
else {
    if (-not (Test-Path $templatePath)) {
        throw "Template nicht gefunden: $templatePath"
    }

    gh release create $Version --target $Target --title $Version --notes-file $templatePath
}

gh release view $Version --json url,name
