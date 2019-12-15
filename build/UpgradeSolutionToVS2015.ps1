# Takes a path to a VS 2013 .sln file, and makes a copy of the solution (and dependent projects) as VS2015.
param([string]$solution)

If ($solution -eq "")
{
    Write-Host "No -solution parameter given!"
    Exit
}

$fileExists = Test-Path $solution
if ($fileExists -eq $False)
{
    Write-Host "$solution not found!"
    Exit
}

$vs2015solution = $solution -replace "vs2013", "vs2015"
Write-Host "Creating $vs2015solution from: $solution"

# Create directory if it doesn't exist
New-Item -ItemType Directory -Force -Path vs2015

# Read contents of file, modify, and write to new location
$slncontents = (Get-Content $solution) 
$slncontents = $slncontents -replace '# Visual Studio 2013', '# Visual Studio 14'
$slncontents = $slncontents -replace 'VisualStudioVersion = 12.0.31101.0', 'VisualStudioVersion = 14.0.24720.0'

# Make copies of the project files
$slncontents | Select-String 'Project\(' |
    ForEach-Object {
        $projectParts = $_ -Split '[,=]' | ForEach-Object { $_.Trim('[ "{}]') };
        $projectName = $projectParts[1]
        
        # Adjust project path as we're not in the same directory as it.
        $projectPath = $projectParts[2]
        
        if ($projectPath.StartsWith('..\'))
        {
            $projectPath = $projectPath.substring(3)
        }
        
        # Upgrade project
        
        # Create directory if it doesn't exist
        $projectItem = Get-ChildItem $projectPath
        $newProjectFile = $($projectItem.BaseName) + '.vcxproj'
        $newProjectPath = $projectItem.DirectoryName

        # Filter file
        $filterFile = $($newProjectFile) + '.filters'
        $filterFile = Join-Path $newProjectPath $filterFile
        
        Join-Path $newProjectPath $newProjectFile
        $vs2015projectPath = $newProjectPath -replace "vs2013", "vs2015"
        $vs2015projectFile = Join-Path $vs2015projectPath $newProjectFile
                
        New-Item -ItemType Directory -Force -Path $vs2015projectPath
        
        # Read contents of file, modify, and write to new location
        $projcontents = (Get-Content $projectPath) 
        $projcontents = $projcontents -replace '<PlatformToolset\>v120</PlatformToolset>', '<PlatformToolset>v140</PlatformToolset>'
        $projcontents = $projcontents -replace 'lib32-msvc-12.0', 'lib32-msvc-14.0'
        $projcontents = $projcontents -replace '2013', '2015'

        $projcontents | Set-Content $vs2015projectFile
        
        # Copy the filter file
        $filterFileExists = Test-Path $filterFile
        if ($filterFileExists -eq $True)
        {
            Copy-Item $filterFile $vs2015projectPath
        }
    }

$slncontents = $slncontents -replace 'vs2013', 'vs2015'
$slncontents | Set-Content $vs2015solution
