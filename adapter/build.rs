use std::{env, fs, path::PathBuf};
fn value<'a>(text:&'a str,name:&str)->&'a str{text.lines().find_map(|line|{let rest=line.trim().strip_prefix("#define ")?;let(key,val)=rest.split_once(' ')?;(key==name).then_some(val.trim())}).unwrap_or_else(||panic!("missing profile macro {name}"))}
fn quoted(v:&str)->&str{let a=v.find('"').expect("quoted profile value")+1;let b=v.rfind('"').expect("quoted profile value");&v[a..b]}
fn main(){
 let manifest=PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap());
 let header=manifest.join("../shared/GameProfile.h");println!("cargo:rerun-if-changed={}",header.display());
 println!("cargo:rerun-if-env-changed=THEATER_NATIVE_LIBRARY_ROOT");
 let native_root=env::var_os("THEATER_NATIVE_LIBRARY_ROOT").map(PathBuf::from).unwrap_or_else(||manifest.join("../build"));
 let shared_lib=native_root.join("shared/Release");println!("cargo:rustc-link-search=native={}",shared_lib.display());println!("cargo:rustc-link-lib=static=GameProfile");
 println!("cargo:rerun-if-changed={}",shared_lib.join("GameProfile.lib").display());
 let render_lib=native_root.join("Release");println!("cargo:rustc-link-search=native={}",render_lib.display());println!("cargo:rustc-link-lib=static=TheaterRenderBackend");
 println!("cargo:rerun-if-changed={}",render_lib.join("TheaterRenderBackend.lib").display());
 for name in ["d3d12","dxgi","d3dcompiler","user32","gdi32","dwmapi","imm32"] {println!("cargo:rustc-link-lib={name}");}
 let text=fs::read_to_string(header).expect("read shared GameProfile.h");
 let profile=quoted(value(&text,"TM_PROFILE_NAME"));let path=quoted(value(&text,"TM_EXPECTED_EXE_PATH")).replace("\\\\","\\");
 let file=quoted(value(&text,"TM_EXPECTED_FILE_VERSION"));let product=quoted(value(&text,"TM_EXPECTED_PRODUCT_VERSION"));let sha=quoted(value(&text,"TM_EXPECTED_SHA256"));
 let generated=format!("pub const PROFILE_NAME:&str=\"{profile}\";\npub const EXPECTED_EXE_PATH:&str=r\"{path}\";\npub const EXPECTED_FILE_VERSION_STR:&str=\"{file}\";\npub const EXPECTED_PRODUCT_VERSION_STR:&str=\"{product}\";\npub const EXPECTED_SHA256_HEX:&str=\"{sha}\";\n");
 // Every layout macro (TM_OFF_*, TM_VAL_*, TM_AOB_*) becomes a Rust constant, so offsets live only in GameProfile.h.
 let mut generated=generated;
 for line in text.lines(){let Some(rest)=line.trim().strip_prefix("#define TM_")else{continue};let Some((key,val))=rest.split_once(char::is_whitespace)else{continue};
  let val=val.split("/*").next().unwrap().trim();
  if key.starts_with("OFF_")||key.starts_with("VAL_"){generated+=&format!("#[allow(dead_code)]pub const {key}:usize={val};
");}
  else if key.starts_with("AOB_"){generated+=&format!("#[allow(dead_code)]pub const {key}:&str=\"{}\";
",quoted(val));}}
 let out=PathBuf::from(env::var_os("OUT_DIR").unwrap()).join("game_profile.rs");fs::write(out,generated).expect("write generated profile");
}
