/** @file pcie_core.c
 *
 * Contains PCIe related functions that are shared between different driver
 * models (e.g. firmware builds, DHD builds, BMAC builds), in order to avoid
 * code duplication.
 *
 * Copyright (C) 1999-2019, Broadcom.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions
 * of the license of that module.  An independent module is a module which is
 * not derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id: pcie_core.c 769591 2018-06-27 00:08:22Z $
 */

#include <bcm_cfg.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <bcmdefs.h>
#include <osl.h>
#include <siutils.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <pcicfg.h>
#include "pcie_core.h"

/* local prototypes */

/* local variables */

/* function definitions */

#ifdef BCMDRIVER

/* wd_mask/wd_val is only for chipc_corerev >= 65 */
void pcie_watchdog_reset(osl_t *osh, si_t *sih, uint32 wd_mask, uint32 wd_val)
{
    uint32 val, i, lsc;
    uint16 cfg_offset[] = {
        PCIECFGREG_STATUS_CMD,        PCIECFGREG_PM_CSR,
        PCIECFGREG_MSI_CAP,           PCIECFGREG_MSI_ADDR_L,
        PCIECFGREG_MSI_ADDR_H,        PCIECFGREG_MSI_DATA,
        PCIECFGREG_LINK_STATUS_CTRL2, PCIECFGREG_RBAR_CTRL,
        PCIECFGREG_PML1_SUB_CTRL1,    PCIECFGREG_REG_BAR2_CONFIG,
        PCIECFGREG_REG_BAR3_CONFIG};
    sbpcieregs_t *pcieregs = NULL;
    uint32 origidx = si_coreidx(sih);

#ifdef BCMFPGA_HW
    if (CCREV(sih->ccrev) < 0x43) {
        /* To avoid hang on FPGA, donot reset watchdog */
        si_setcoreidx(sih, origidx);
        return;
    }
#endif // endif

    /* Switch to PCIE2 core */
    pcieregs = (sbpcieregs_t *)si_setcore(sih, PCIE2_CORE_ID, 0);
    BCM_REFERENCE(pcieregs);
    ASSERT(pcieregs != NULL);

    /* Disable/restore ASPM Control to protect the watchdog reset */
    W_REG(osh, &pcieregs->configaddr, PCIECFGREG_LINK_STATUS_CTRL);
    lsc = R_REG(osh, &pcieregs->configdata);
    val = lsc & (~PCIE_ASPM_ENAB);
    W_REG(osh, &pcieregs->configdata, val);

    if (CCREV(sih->ccrev) >= 0x41) {
        si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, watchdog), wd_mask,
                   wd_val);
        si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, watchdog),
                   WD_COUNTER_MASK, 0x4);
#ifdef BCMQT_HW
        OSL_DELAY(0x7D0 * 0xFA0);
#else
        OSL_DELAY(0x7D0); /* 2 ms */
#endif // endif
        val =
            si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, intstatus), 0, 0);
        si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, intstatus), wd_mask,
                   val & wd_mask);
    } else {
        si_corereg_writeonly(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, watchdog),
                             ~0, 0x4);
        /* Read a config space to make sure the above write gets flushed on PCIe
         * bus */
        val = OSL_PCI_READ_CONFIG(osh, PCI_CFG_VID, sizeof(uint32));
        OSL_DELAY(0x186A0);
    }

    W_REG(osh, &pcieregs->configaddr, PCIECFGREG_LINK_STATUS_CTRL);
    W_REG(osh, &pcieregs->configdata, lsc);

    if (sih->buscorerev <= 0xD) {
        /* Write configuration registers back to the shadow registers
         * cause shadow registers are cleared out after watchdog reset.
         */
        for (i = 0; i < ARRAYSIZE(cfg_offset); i++) {
            W_REG(osh, &pcieregs->configaddr, cfg_offset[i]);
            val = R_REG(osh, &pcieregs->configdata);
            W_REG(osh, &pcieregs->configdata, val);
        }
    }
    si_setcoreidx(sih, origidx);
}

/* CRWLPCIEGEN2-117 pcie_pipe_Iddq should be controlled
 * by the L12 state from MAC to save power by putting the
 * SerDes analog in IDDQ mode
 */
void pcie_serdes_iddqdisable(osl_t *osh, si_t *sih, sbpcieregs_t *sbpcieregs)
{
    sbpcieregs_t *pcie = NULL;
    uint crwlpciegen2_117_disable = 0;
    uint32 origidx = si_coreidx(sih);

    crwlpciegen2_117_disable = PCIE_PipeIddqDisable0 | PCIE_PipeIddqDisable1;
    /* Switch to PCIE2 core */
    pcie = (sbpcieregs_t *)si_setcore(sih, PCIE2_CORE_ID, 0);
    BCM_REFERENCE(pcie);
    ASSERT(pcie != NULL);

    OR_REG(osh, &sbpcieregs->control, crwlpciegen2_117_disable);

    si_setcoreidx(sih, origidx);
}

#define PCIE_PMCR_REFUP_MASK 0x3f0001e0
#define PCIE_PMCR_REFEXT_MASK 0x400000
#define PCIE_PMCR_REFUP_100US 0x38000080
#define PCIE_PMCR_REFEXT_100US 0x400000

/* Set PCIE TRefUp time to 100us */
void pcie_set_trefup_time_100us(si_t *sih)
{
    si_corereg(sih, sih->buscoreidx, OFFSETOF(sbpcieregs_t, configaddr), ~0,
               PCI_PMCR_REFUP);
    si_corereg(sih, sih->buscoreidx, OFFSETOF(sbpcieregs_t, configdata),
               PCIE_PMCR_REFUP_MASK, PCIE_PMCR_REFUP_100US);

    si_corereg(sih, sih->buscoreidx, OFFSETOF(sbpcieregs_t, configaddr), ~0,
               PCI_PMCR_REFUP_EXT);
    si_corereg(sih, sih->buscoreidx, OFFSETOF(sbpcieregs_t, configdata),
               PCIE_PMCR_REFEXT_MASK, PCIE_PMCR_REFEXT_100US);
}

#endif /* BCMDRIVER */
