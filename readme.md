### TODO:
- [x] handle comments and strings
- [x] ignore URLs
- [x] direct folders to init.lua/luau
- [x] automatically destroy previous version of the bundle
- [x] config file that contains the root file path and output path (or multiple?)
  - [ ] overridable config values, depths would be (in ascending importance): project, bundle, launch-argument
- [x] support for larger bundles
- [ ] dont load modules immedietly
- [ ] generate a source map for each bundle
  - [ ] catch errors and automatically use the source map on it
- [ ] turn global functions into locals to prevent shadowing and GlobalUsedAsLocal warnings?

### scary TODO:
- [ ] multi threading support?
