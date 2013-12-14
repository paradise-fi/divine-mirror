{ nixpkgs ? <nixpkgs>, divineSrc, release ? false, buildType ? "Debug" }:

let
  pkgs = import nixpkgs {};
  debuild = args:
    import ./nix/debian_build.nix ({ stdenv = pkgs.stdenv; vmTools = pkgs.vmTools; } // args);
  rpmbuild = pkgs.releaseTools.rpmBuild;
  rpmbuild_i386 = pkgs.pkgsi686Linux.releaseTools.rpmBuild;
  vmImgs = pkgs.vmTools.diskImageFuns;
  lib = pkgs.lib;

  wimlib = pkgs.callPackage nix/wimlib.nix {};
  windows7_iso = pkgs.fetchurl {
    url = "http://msft.digitalrivercontent.net/win/X17-59183.iso";
    sha256 = "13l3skfp3qi2ccv9djhpg7a7f2g57rph8n38dnkw8yh8w1bdyk7x";
  };

  windows7_img = pkgs.callPackage nix/windows_img.nix {
    inherit wimlib;
    iso = windows7_iso;
    name = "windows7";
    vmTools = import "${nixpkgs}/pkgs/build-support/vm/default.nix" {
       inherit pkgs;
       rootModules = [ "virtio_net" "virtio_pci" "virtio_blk" "virtio_balloon"
                       "9p" "9pnet_virtio" "ext4" "fuse" "loop" "udf" ];
    };
  };

  windows_cmake = pkgs.callPackage nix/windows_cmake.nix {};
  windows_mingw = pkgs.callPackage nix/windows_mingw.nix {};
  windows_nsis = pkgs.callPackage nix/windows_nsis.nix {};
  windows_qt = pkgs.callPackage nix/windows_qt.nix {
      windows_img = windows7_img; inherit pkgs windows_mingw; };
  windows_python = pkgs.callPackage nix/windows_python.nix {};
  windows_llvm = pkgs.callPackage nix/windows_llvm.nix {
      windows_img = windows7_img; inherit pkgs windows_mingw windows_cmake windows_python; };

  extra_debs = [ "cmake" "build-essential" "debhelper" "m4"
                 "libqt4-dev" "libboost-dev" "libncurses5-dev"
                 "binutils-gold" ];
  extra_debs31 = extra_debs ++ [ "llvm-3.1-dev" ];
  extra_debs32 = extra_debs ++ [ "llvm-3.2-dev" "clang-3.2" ];
  extra_rpms = [ "cmake" "redhat-rpm-config" ];

  mkVM = { VM, extras, diskFun, mem ? 3072 }:
   VM rec {
     name = "divine";
     src = jobs.tarball;
     diskImage = diskFun { extraPackages = extras; size = 8192; };
     configurePhase = ''
          echo "-DCMAKE_BUILD_TYPE=${buildType}" > pkgbuildflags
          echo "override_dh_auto_test:" >> debian/rules
          echo "	dh_auto_test || touch $out/nix-support/failed" >> debian/rules
          sed -e "s,^make check$,make check || touch $out/nix-support/failed," -i divine.spec
     '';
     doCheck = false; # the package builder is supposed to run checks
     memSize = mem;
   };

  mkbuild = { name, inputs,
              flags ? [ "-DCOMPRESSION=OFF" "-DHASH_COMPACTION=OFF" "-DEXPLICIT=OFF" ],
              clang ? false
            }:
            { system ? builtins.currentSystem }:
    let pkgs = import nixpkgs { inherit system; };
        cmdflags = [ "-DCMD_GCC=${pkgs.gcc}/bin/gcc" ] ++
                   (if lib.eqStrings name "llvm" || lib.eqStrings name "full" ||
                       lib.eqStrings name "medium"
                      then [ "-DCMD_CLANG=${pkgs.clangUnwrapped}/bin/clang"
                             "-DCMD_AR=${pkgs.binutils_gold}/bin/ar"
                             "-DCMD_GOLD=${pkgs.binutils_gold}/bin/ld.gold"
                             "-DCMD_LLVMGOLD=${pkgs.llvm}/lib/LLVMgold.so" ]
                      else []);
        profile = if lib.eqStrings buildType "Debug" && !clang
                     then [ "-DPROFILE=ON" "-DGCOV=${pkgs.gcc47.gcc}/bin/gcov" ] else [];
        compiler = if clang
                      then [ "-DCMAKE_CXX_COMPILER=${pkgs.clangSelf}/bin/clang++"
                             "-DCMAKE_C_COMPILER=${pkgs.clangSelf}/bin/clang" ]
                      else [];
    in pkgs.releaseTools.nixBuild {
       name = "divine-" + name;
       src = jobs.tarball;
       buildInputs = [ pkgs.gcc47 pkgs.cmake pkgs.perl pkgs.m4 pkgs.lcov ] ++ inputs { inherit pkgs; };
       cmakeFlags = [ "-DCMAKE_BUILD_TYPE=${buildType}" ] ++ compiler ++ cmdflags ++ profile ++ flags;
       dontStrip = true;
       checkPhase = ''
          make unit || touch $out/nix-support/failed
          make functional || touch $out/nix-support/failed
          cp -R test/results $out/test-results && \
            echo "report tests $out/test-results" >> $out/nix-support/hydra-build-products || true
          make lcov-report && \
            cp -R lcov-report $out/ && \
            echo "report coverage $out/lcov-report" >> $out/nix-support/hydra-build-products || \
            true
       '';
    };

  mkwin = image: flags: with_llvm: pkgs.callPackage nix/windows_build.nix {
    inherit windows_mingw;
    tools = [ windows_cmake windows_nsis windows_qt ] ++ (if with_llvm then [ windows_llvm ] else []);
    img = image;
    src = jobs.tarball;
    name = "divine";
    mem = "2048M";
    buildScript = ''
      set -ex
      mkdir build && cd build
      # Windows/mingw breaks on big files :-(
      bt=${buildType}
      test "$bt" = "RelWithDebInfo" && echo ${flags} | grep -v SMALL && bt=Release
      cmake -LA -G "MSYS Makefiles" \
        -DQT_UIC_EXECUTABLE=$QTDIR/bin/uic.exe \
        -DQT_RCC_EXECUTABLE=$QTDIR/bin/rcc.exe \
        -DQT_MOC_EXECUTABLE=$QTDIR/bin/moc.exe \
        -DQT_QCOLLECTIONGENERATOR_EXECUTABLE=$QTDIR/bin/qcollectiongenerator.exe \
        -DQT_INCLUDE_DIR=$QTDIR/include \
        -DQT_QTCORE_INCLUDE_DIR=$QTDIR/include/QtCore \
        -DQT_QTGUI_INCLUDE_DIR=$QTDIR/include/QtGui \
        -DQT_QTXML_INCLUDE_DIR=$QTDIR/include/QtXml \
        -DLLVM_INCLUDE_DIRS=D:\\llvm\\include \
        -DLLVM_LIBRARY_DIRS=D:\\llvm\\lib \
        -DRX_PATH=D:\\mingw\\include \
        -BUILD_DCMAKE_TYPE=$bt ${flags} ../source
      make VERBOSE=1
      mkdir E:\\nix-support
      make unit || touch E:\\nix-support\\failed
      make functional || touch E:\\nix-support\\failed
      make package || touch E:\\nix-support\\failed
      cp tools/divine.exe E:/
      cp divine-*.exe E:/ || true
    '';
  };

  versionFile = builtins.readFile ./divine/utility/version.cpp;
  versionLine = builtins.head (
    lib.filter (str: lib.eqStrings (builtins.substring 0 22 str) "#define DIVINE_VERSION")
               (lib.splitString "\n" versionFile));
  version = builtins.head (builtins.tail (lib.splitString "\"" (versionLine + " ")));

  jobs = rec {

    tarball = pkgs.releaseTools.sourceTarball rec {
        inherit version;
        name = "divine-tarball";
        versionSuffix = if divineSrc ? revCount
                           then "+pre${toString divineSrc.revCount}"
                           else "";
        src = divineSrc;
        buildInputs = (with pkgs; [ cmake gcc47 ]);
        cmakeFlags = [ "-DVERSION_APPEND=${versionSuffix}" ];
        dontFixCmake = true;
        autoconfPhase = ''
          sed -e "s,^\(Version:.*\)0$,\1${version}${versionSuffix}," -i divine.spec
          sed -e 's,"","${versionSuffix}",' -i cmake/VersionAppend.cmake

          mv debian/changelog debian/changelog.xxx
          echo "divine (${version}${versionSuffix}) unstable; urgency=low" >> debian/changelog
          echo >> debian/changelog
          echo "  * Automated Hydra build" >> debian/changelog
          echo >> debian/changelog
          echo " -- Petr Rockai <mornfall@debian.org>  `date -R`" >> debian/changelog
          echo >> debian/changelog
          cat debian/changelog.xxx >> debian/changelog
          rm debian/changelog.xxx

          chmod +x configure # ha-ha
        '';
        distPhase = ''
            make package_source
            mkdir $out/tarballs
            cp divine-*.tar.gz $out/tarballs
        '';
      };

    manual =
     let tex = pkgs.texLiveAggregationFun { paths = [ pkgs.texLive pkgs.lmodern ]; };
          in pkgs.releaseTools.nixBuild {
              name = "divine-manual";
              src = jobs.tarball;
              buildInputs = [ pkgs.gcc47 pkgs.cmake pkgs.perl pkgs.haskellPackages.pandoc tex ];
              buildPhase = "make manual website";
              installPhase = ''
                mkdir $out/manual $out/website
                cp manual/manual.pdf manual/manual.html $out/manual/
                cp website/*.html website/*.png website/*.css $out/website/
                cp ../website/template.html $out/website
              '';
              checkPhase = ":";
          };

    gcc_minimal = mkbuild { name = "minimal"; inputs = { pkgs }: []; };
    gcc_mpi = mkbuild { name = "mpi"; inputs = { pkgs }: [ pkgs.openmpi ]; };
    gcc_gui = mkbuild { name = "gui"; inputs = { pkgs }: [ pkgs.qt4 ]; };
    gcc_llvm = mkbuild { name = "llvm"; inputs = { pkgs }: [ pkgs.llvm pkgs.clang ]; };
    gcc_timed = mkbuild { name = "timed"; inputs = { pkgs }: [ pkgs.libxml2 pkgs.boost ]; };
    gcc_compression = mkbuild { name = "compression"; inputs = { pkgs }: [];
                       flags = [ "-DHASH_COMPACTION=OFF" "-DCOMPRESSION=ON" "-DEXPLICIT=OFF" ]; };
    gcc_hashcompaction = mkbuild { name = "hashcompaction"; inputs = { pkgs }: [];
                       flags = [ "-DCOMPRESSION=OFF" "-DHASH_COMPACTION=ON" "-DEXPLICIT=OFF" ]; };
    gcc_explicit = mkbuild { name = "explicit"; inputs = { pkgs }: [];
                       flags = [ "-DCOMPRESSION=OFF" "-DHASH_COMPACTION=OFF" "-DEXPLICIT=ON" ]; };
    gcc_full = mkbuild { name = "full"; inputs = { pkgs }:
                          [ pkgs.openmpi pkgs.llvm pkgs.clang pkgs.qt4 pkgs.libxml2 pkgs.boost ];
                         flags = []; };
    clang_minimal = mkbuild { name = "minimal"; inputs = { pkgs }: []; clang = true; };
    clang_medium = mkbuild { name = "medium"; inputs = { pkgs }:
                              [ pkgs.openmpi pkgs.libcxxLLVM pkgs.clangSelf pkgs.libxml2 ];
                             flags = []; clang = true; };

    debian70_i386 = mkVM { VM = debuild; diskFun = vmImgs.debian70i386; extras = extra_debs31; };
    ubuntu1210_i386 = mkVM { VM = debuild; diskFun = vmImgs.ubuntu1210i386; extras = extra_debs31; };
    ubuntu1304_i386 = mkVM { VM = debuild; diskFun = vmImgs.ubuntu1304i386; extras = extra_debs32; };
    fedora18_i386 = mkVM { VM = rpmbuild_i386; diskFun = vmImgs.fedora18i386; extras = extra_rpms;
                           mem = 2047; };

    debian70_x86_64 = mkVM { VM = debuild; diskFun = vmImgs.debian70x86_64; extras = extra_debs31; };
    ubuntu1210_x86_64 = mkVM { VM = debuild; diskFun = vmImgs.ubuntu1210x86_64; extras = extra_debs31; };
    ubuntu1304_x86_64 = mkVM { VM = debuild; diskFun = vmImgs.ubuntu1304x86_64; extras = extra_debs32; };
    fedora18_x86_64 = mkVM { VM = rpmbuild; diskFun = vmImgs.fedora18x86_64; extras = extra_rpms; };

    win7_i386_small = mkwin windows7_img "-DSMALL=ON" false;
    win7_i386 = mkwin windows7_img "" false;
    win7_i386_llvm = mkwin windows7_img "" true;
  };
in
  jobs
