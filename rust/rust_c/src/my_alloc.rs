use cty::c_void;

struct KTAllocator;

extern "C" {
    pub fn pvPortMalloc(size: usize) -> *mut cty::c_void;
    pub fn vPortFree(p: *mut cty::c_void);
}

#[cfg(not(test))]
unsafe impl core::alloc::GlobalAlloc for KTAllocator {
    unsafe fn alloc(&self, layout: core::alloc::Layout) -> *mut u8 {
        let ptr = pvPortMalloc(layout.size());
        ptr as *mut u8
    }
    unsafe fn dealloc(&self, ptr: *mut u8, _layout: core::alloc::Layout) {
        vPortFree(ptr as _)
    }
}

#[cfg(not(test))]
#[global_allocator]
static KT_ALLOCATOR: KTAllocator = KTAllocator;
