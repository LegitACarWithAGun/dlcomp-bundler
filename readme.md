# dlcomp-bundler
heavily WIP, unsure if Im even keeping this name.  
probably shouldve just forked an existing bundler.  
see https://github.com/LegitACarWithAGun/deadlinecomp-mods for a project using this tool

### TODO:
- [x] handle comments and strings
- [x] ignore URLs
- [x] direct folders to init.lua/luau
- [x] automatically destroy previous version of the bundle
- [x] config file that contains the root file path and output path (or multiple?)
  - [x] overridable config values, depths would be (in ascending importance): project, bundle, launch-argument
  - [ ] cli flags
- [x] support for larger bundles
- [x] dont load modules immedietly
- [ ] generate a source map for each bundle
  - [ ] catch errors and automatically use the source map on it
- [x] turn global functions into locals to prevent shadowing and GlobalUsedAsLocal warnings?
- [ ] only parse each file once (per bundle, maybe can share it across bundles aswell?)

### scary TODO:
- [ ] multi threading support?
