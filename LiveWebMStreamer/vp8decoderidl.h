

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 7.00.0555 */
/* at Fri Oct 08 14:01:37 2010
 */
/* Compiler settings for ..\IDL\vp8decoder.idl:
    Oicf, W1, Zp8, env=Win32 (32b run), target_arch=X86 7.00.0555 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif // __RPCNDR_H_VERSION__


#ifndef __vp8decoderidl_h__
#define __vp8decoderidl_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IVP8PostProcessing_FWD_DEFINED__
#define __IVP8PostProcessing_FWD_DEFINED__
typedef interface IVP8PostProcessing IVP8PostProcessing;
#endif 	/* __IVP8PostProcessing_FWD_DEFINED__ */


#ifndef __VP8Decoder_FWD_DEFINED__
#define __VP8Decoder_FWD_DEFINED__

#ifdef __cplusplus
typedef class VP8Decoder VP8Decoder;
#else
typedef struct VP8Decoder VP8Decoder;
#endif /* __cplusplus */

#endif 	/* __VP8Decoder_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 



#ifndef __VP8DecoderLib_LIBRARY_DEFINED__
#define __VP8DecoderLib_LIBRARY_DEFINED__

/* library VP8DecoderLib */
/* [version][helpstring][uuid] */ 


enum VP8PostProcessingFlags
    {	VP8None	= 0,
	VP8Deblock	= 0x1,
	VP8Demacroblock	= 0x2,
	VP8AddNoise	= 0x4
    } ;

EXTERN_C const IID LIBID_VP8DecoderLib;

#ifndef __IVP8PostProcessing_INTERFACE_DEFINED__
#define __IVP8PostProcessing_INTERFACE_DEFINED__

/* interface IVP8PostProcessing */
/* [helpstring][uuid][object] */ 


EXTERN_C const IID IID_IVP8PostProcessing;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("ED3110F2-5211-11DF-94AF-0026B977EEAA")
    IVP8PostProcessing : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE SetFlags( 
            /* [in] */ int PostProcessingFlags) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetFlags( 
            /* [out] */ int *pPostProcessingFlags) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetDeblockingLevel( 
            /* [in] */ int DeblockingLevel) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetDeblockingLevel( 
            /* [out] */ int *pDeblockingLevel) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetNoiseLevel( 
            /* [in] */ int NoiseLevel) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetNoiseLevel( 
            /* [out] */ int *pNoiseLevel) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE ApplyPostProcessing( void) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVP8PostProcessingVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVP8PostProcessing * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            __RPC__deref_out  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVP8PostProcessing * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVP8PostProcessing * This);
        
        HRESULT ( STDMETHODCALLTYPE *SetFlags )( 
            IVP8PostProcessing * This,
            /* [in] */ int PostProcessingFlags);
        
        HRESULT ( STDMETHODCALLTYPE *GetFlags )( 
            IVP8PostProcessing * This,
            /* [out] */ int *pPostProcessingFlags);
        
        HRESULT ( STDMETHODCALLTYPE *SetDeblockingLevel )( 
            IVP8PostProcessing * This,
            /* [in] */ int DeblockingLevel);
        
        HRESULT ( STDMETHODCALLTYPE *GetDeblockingLevel )( 
            IVP8PostProcessing * This,
            /* [out] */ int *pDeblockingLevel);
        
        HRESULT ( STDMETHODCALLTYPE *SetNoiseLevel )( 
            IVP8PostProcessing * This,
            /* [in] */ int NoiseLevel);
        
        HRESULT ( STDMETHODCALLTYPE *GetNoiseLevel )( 
            IVP8PostProcessing * This,
            /* [out] */ int *pNoiseLevel);
        
        HRESULT ( STDMETHODCALLTYPE *ApplyPostProcessing )( 
            IVP8PostProcessing * This);
        
        END_INTERFACE
    } IVP8PostProcessingVtbl;

    interface IVP8PostProcessing
    {
        CONST_VTBL struct IVP8PostProcessingVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVP8PostProcessing_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IVP8PostProcessing_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IVP8PostProcessing_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IVP8PostProcessing_SetFlags(This,PostProcessingFlags)	\
    ( (This)->lpVtbl -> SetFlags(This,PostProcessingFlags) ) 

#define IVP8PostProcessing_GetFlags(This,pPostProcessingFlags)	\
    ( (This)->lpVtbl -> GetFlags(This,pPostProcessingFlags) ) 

#define IVP8PostProcessing_SetDeblockingLevel(This,DeblockingLevel)	\
    ( (This)->lpVtbl -> SetDeblockingLevel(This,DeblockingLevel) ) 

#define IVP8PostProcessing_GetDeblockingLevel(This,pDeblockingLevel)	\
    ( (This)->lpVtbl -> GetDeblockingLevel(This,pDeblockingLevel) ) 

#define IVP8PostProcessing_SetNoiseLevel(This,NoiseLevel)	\
    ( (This)->lpVtbl -> SetNoiseLevel(This,NoiseLevel) ) 

#define IVP8PostProcessing_GetNoiseLevel(This,pNoiseLevel)	\
    ( (This)->lpVtbl -> GetNoiseLevel(This,pNoiseLevel) ) 

#define IVP8PostProcessing_ApplyPostProcessing(This)	\
    ( (This)->lpVtbl -> ApplyPostProcessing(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IVP8PostProcessing_INTERFACE_DEFINED__ */


EXTERN_C const CLSID CLSID_VP8Decoder;

#ifdef __cplusplus

class DECLSPEC_UUID("ED3110F3-5211-11DF-94AF-0026B977EEAA")
VP8Decoder;
#endif
#endif /* __VP8DecoderLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


