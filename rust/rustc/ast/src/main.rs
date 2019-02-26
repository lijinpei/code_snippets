#![feature(rustc_private)]

/// need to set-up a -L command line option (or some env-vars?) to run this program

extern crate rustc;
extern crate rustc_driver;
extern crate syntax;

use rustc_driver::{CompilerCalls, driver::{CompileController, CompileState}, getopts::Matches};
use rustc::session::Session;
use std::sync::{Arc, Mutex};

pub struct MyControllerInternal {
    parsed_crate: Option<syntax::ast::Crate>,
    expanded_crate: Option<syntax::ast::Crate>,

}

// rustc span are !Send because interner are !Send
// we will take care of the internal and consider span and ast Send
unsafe impl Send for MyControllerInternal {
}

impl MyControllerInternal {
    fn new() -> MyControllerInternal {
        MyControllerInternal {
            parsed_crate: None,
            expanded_crate: None,
        }
    }
}

pub struct MyController {
    intern: Arc<Mutex<MyControllerInternal>>,
}

impl MyController {
    pub fn new(intern: Arc<Mutex<MyControllerInternal>>) -> MyController {
        MyController {
            intern
        }
    }
}

pub fn print_item_kind(ik: &syntax::ast::ItemKind) {
    /*
    match ik {
        ExternCrate(_) => {
            eprint!("ExternCrate");
        }

    }
    */
    eprint!("{:?}", ik);
}

pub fn visit_macro_items(_st: &CompileState, krate: &syntax::ast::Crate) {
    for item in krate.module.items.iter() {
        eprint!("find item {} id {} ", item.ident, item.id.as_usize());
        print_item_kind(&item.node);
        eprintln!();
    }
}

pub fn dump_ast(st: &CompileState, krate: &syntax::ast::Crate) {
    use rustc_driver::pretty::PpMode;
    use rustc_driver::pretty::PpSourceMode;
    rustc_driver::pretty::print_after_parsing(st.session, st.input, krate, PpMode::PpmSource(PpSourceMode::PpmNormal), None);
}

impl<'a> CompilerCalls<'a> for MyController {
    fn build_controller(self:Box<Self>, _sess: &Session, _mat: &Matches) -> CompileController<'a> {
        let mut ret = CompileController::basic();
        // ret.keep_ast = true; // keep ast after analysis

        {
            let intern = self.intern.clone();
            ret.after_parse.callback = Box::new(
            move |st| {
                match st.krate {
                    Some(ref krate) => {
                        (*intern.lock().unwrap()).parsed_crate = Some(krate.clone());
                    },
                    None => (),
                }
            }
            );
        }

        {
            let intern = self.intern.clone();
            ret.after_expand.callback = Box::new(
            move |st| {
                match st.expanded_crate {
                    Some(ref krate) => {
                        (*intern.lock().unwrap()).expanded_crate = Some(Clone::clone(krate));
                    },
                    None => (),
                }
            }
            );
        }
        ret
    }
}

fn main() {
    let intern = Arc::new(Mutex::new(MyControllerInternal::new()));
//    rustc_driver::run(|| {
    syntax::with_globals(||{
        let args:Vec<String> = std::env::args().into_iter().collect();
        rustc_driver::run_compiler(&args, Box::new(MyController::new(intern)), None, None);
    });
//    });
}
