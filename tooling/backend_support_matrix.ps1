# Compile each generated test-object TU against each backend; emit a support matrix.
$ErrorActionPreference = "Continue"
$bd = "D:/dev/avendish/build/llvm_msvc-Debug"
Set-Location $bd
$boost = "D:/dev/ossia/score/3rdparty/libossia/3rdparty/boost_1_90_0"
$fmt = "$bd/_deps/fmt-src/include"
$common = @("-I$bd","-ID:/dev/avendish","-I$bd/_deps/qlibs_reflect-src","-ID:/dev/avendish/include","-I$bd/_deps/magic_enum-src/include","-I$fmt","-I$boost")
$max = "D:/dev/max-sdk-base/c74support"
$inc = @{
  dump = $common + @("-I$bd/_deps/yyjson-src/src")
  max  = $common + @("-I$max/max-includes","-I$max/msp-includes","-I$max/jit-includes")
  pd   = $common + @("-ID:/apps/pd-0.55-1/src")
  td   = $common + @("-ID:/dev/CustomOperatorSamples/include")
}
$defs = @{
  dump=@(); max=@("-DAVND_MAXMSP=1","-DC74_USE_STRICT_TYPES=1"); pd=@(); td=@()
}
# generated-TU suffix glob per backend
$suffix = @{ dump="_dump.cpp"; max="_max.cpp"; pd="_pd.cpp"; td="_*touchdesigner.cpp" }
$cl = "D:/llvm/bin/clang-cl.exe"

# discover all test classes from dump TUs (every object has a dump TU)
$classes = Get-ChildItem -Filter "examples__tests__*_dump.cpp" | ForEach-Object {
  ($_.BaseName -replace "^examples__tests__","" -replace "_dump$","")
} | Sort-Object

$rows = @()
foreach($cls in $classes){
  $row = [ordered]@{ object = $cls }
  foreach($b in "dump","max","pd","td"){
    $f = Get-ChildItem -Filter ("examples__tests__" + $cls + $suffix[$b]) -ErrorAction SilentlyContinue | Select-Object -First 1
    if(-not $f){ $row[$b] = "-"; continue }   # not registered for this backend
    $out = & $cl /nologo -DNOMINMAX=1 -DWIN32_LEAN_AND_MEAN=1 ($defs[$b]) /EHsc /Od -clang:-std=c++2c -Zc:__cplusplus /bigobj -W0 ($inc[$b]) -c $f.Name -Fo:NUL 2>&1
    if($out | Select-String -Pattern "error:" -Quiet){ $row[$b] = "FAIL" } else { $row[$b] = "ok" }
  }
  $rows += [pscustomobject]$row
  Write-Output ("{0,-34} dump={1,-4} max={2,-4} pd={3,-4} td={4}" -f $cls,$row.dump,$row.max,$row.pd,$row.td)
}
$rows | ConvertTo-Json | Out-File "$env:TEMP/support_matrix.json" -Encoding utf8
Write-Output "MATRIX_DONE ($($rows.Count) objects)"
