# LCR-II _VT-100_Win32
Programme conçu par Patrice Waechter-Ebling ©2002 
 ---- 
 Mise a jour 2025
# Patch
  peut planter sous Windows11 25h2 
+ push	 esi
+ mov	 esi, ecx
+ test	 BYTE PTR ___flags$[esp], 1
+ je	 SHORT $L96672
+ push	 esi
+ call	 ??3@YAXPAX@Z
+ pop	 ecx
+ mov	 eax, esi
+ pop	 esi
+ call	 __EH_prolog
+ sub	 esp, 512		; 00000200H
+ push	 ebx
+ push	 esi
+ mov	 esi, ecx
+ push	 ebx
+ test	 eax, eax
+ jne	 SHORT $L96574
+ push	 -1
+ push	 ebx
+ test	 eax, eax
+ push	 100			; 00000064H
+ mov	 ecx, esi
+ mov	 DWORD PTR [esp], OFFSET FLAT:??_C@_0CM@LIEEDAII@Applications?5locales?5g?in?ir?ies?5pa@
+ push	 eax
+ mov	 ecx, esi
+ push	 ebx
+ mov	 eax, DWORD PTR [eax+8]
+ DWORD PTR [esi+28], eax
+ ret	 4
  
