#![feature(rustc_private)]

/// need to set-up a -L command line option (or some env-vars?) to run this program

extern crate rustc;
extern crate rustc_driver;
extern crate syntax;

use rustc_driver::{CompilerCalls, driver::{CompileController, CompileState}, getopts::Matches};
use rustc::session::Session;

struct MyController;

fn dump_ast(st: &CompileState, krate: &syntax::ast::Crate) {
    use rustc_driver::pretty::PpMode;
    use rustc_driver::pretty::PpSourceMode;
    rustc_driver::pretty::print_after_parsing(st.session, st.input, krate, PpMode::PpmSource(PpSourceMode::PpmNormal), None);
}

impl<'a> CompilerCalls<'a> for MyController {
    fn build_controller(self:Box<Self>, sess: &Session, mat: &Matches) -> CompileController<'a> {
        let gen_cb = |phase: &str| {
            let phase = phase.to_string();
            move |st: &mut CompileState| {
            eprintln!("phase: {}", phase);
            eprintln!("krate:");
            match st.krate {
                Some(ref krate) => {
                    dump_ast(st, krate);
                },
                None => {
                    eprintln!("None");
                },
            }
            eprintln!("expanded_crate:");
            match st.expanded_crate {
                Some(krate) => {
                    dump_ast(st, krate);
                },
                None => {
                    eprintln!("None");
                },
            }
            }
        };

        let mut ret = CompileController::basic();
        // ret.keep_ast = true; // keep ast after analysis

        ret.after_parse.callback = Box::new(gen_cb("after_parse"));
        ret.after_expand.callback = Box::new(gen_cb("after_expand"));
        ret.after_hir_lowering.callback = Box::new(gen_cb("after_hir_lowering"));
        ret.after_analysis.callback = Box::new(gen_cb("after_analysis"));
        ret.compilation_done.callback = Box::new(gen_cb("compilation_done"));
        ret
    }
}

fn main() {
    rustc_driver::run(|| {
        let args:Vec<String> = std::env::args().into_iter().collect();
        rustc_driver::run_compiler(&args, Box::new(MyController), None, None)
    });
}
