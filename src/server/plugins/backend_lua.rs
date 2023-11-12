use super::Backend;
use mlua::prelude::*;

pub struct BackendLua {
    lua: Lua,
}

impl BackendLua {
    pub fn new() -> Self {
        let lua = Lua::new();

        Self {
            lua,
        }
    }
}

impl Backend for BackendLua {
    fn load(&mut self, code: String) -> anyhow::Result<()> {
        self.lua.load(code).exec().map_err(|e| { error!("[LUA] {:?}", e); e })?;
        Ok(())
    }

    fn load_api(&mut self) -> anyhow::Result<()> {
        let print_fn = self.lua.create_function(|_lua, (msg,): (String,)| {
            info!("[LUA] {}", msg);
            Ok(())
        })?;

        let api = self.lua.create_table()?;

        api.set("print", print_fn)?;

        self.lua.globals().set("MP", api)?;

        Ok(())
    }
}
