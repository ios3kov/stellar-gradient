use after_effects as ae;
use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};

#[derive(Eq, PartialEq, Hash, Clone, Copy, Debug)]
enum Params {
    RegisterLate,
}

#[derive(Default)]
struct Plugin;

ae::define_effect!(Plugin, (), Params);

type RegisterLateFn = unsafe extern "C" fn() -> i32;
static REGISTER_LATE_FN: AtomicPtr<c_void> = AtomicPtr::new(std::ptr::null_mut());

#[unsafe(no_mangle)]
pub extern "C" fn AEHotLoaderCore_SetRegisterFn(ptr: *mut c_void) {
    REGISTER_LATE_FN.store(ptr, Ordering::Release);
}

fn register_late_effect() -> i32 {
    let ptr = REGISTER_LATE_FN.load(Ordering::Acquire);
    if ptr.is_null() {
        return -1001;
    }

    let callback: RegisterLateFn = unsafe { std::mem::transmute(ptr) };
    unsafe { callback() }
}

impl AdobePluginGlobal for Plugin {
    fn params_setup(
        &self,
        params: &mut ae::Parameters<Params>,
        _: ae::InData,
        _: ae::OutData,
    ) -> Result<(), Error> {
        params.add(
            Params::RegisterLate,
            "Hot Load",
            ae::ButtonDef::setup(|button| {
                button.set_label("Register Late Effect");
            }),
        )?;
        Ok(())
    }

    fn handle_command(
        &mut self,
        cmd: ae::Command,
        _: ae::InData,
        mut out_data: ae::OutData,
        params: &mut ae::Parameters<Params>,
    ) -> Result<(), ae::Error> {
        match cmd {
            ae::Command::About => {
                out_data.set_return_msg(
                    "AE Hot Loader macOS PoC\rTests late effect registration without restarting After Effects.",
                );
            }
            ae::Command::UserChangedParam { param_index }
                if params.type_at(param_index) == Params::RegisterLate =>
            {
                let rc = register_late_effect();
                if rc == 0 {
                    out_data.set_return_msg(
                        "Late registration callback returned SUCCESS.\rCheck Effect > AE Hot Loader.",
                    );
                } else {
                    out_data.set_return_msg(&format!(
                        "Late registration callback failed. code={rc}\rSee /tmp/ae-hot-loader.log"
                    ));
                }
                out_data.set_out_flag(ae::OutFlags::DisplayErrorMessage, true);
            }
            ae::Command::Render {
                in_layer,
                mut out_layer,
            } => {
                out_layer.copy_from(&in_layer, None, None)?;
            }
            _ => {}
        }
        Ok(())
    }
}
