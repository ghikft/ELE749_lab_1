library ieee;

use ieee.std_logic_1164.all;
entity Labo1 is 
	port (clock_50 : in 	std_logic;							--clock_50 de DE1soc_pin_assignment
			key		: in 	std_logic_vector(3 downto 0);	--key(0) (defini comme vecteur), pour reset
			sw	: in 	std_logic_vector(9 downto 0);	--s(0) a sw(9)
			ledr		: out	std_logic_vector(1 downto 0);	--leds(0) a leds(1)
			--key	: in	std_logic_vector(3 downto 0);	--
			hex0	: out	std_logic_vector(6 downto 0);	--
			hex1	: out	std_logic_vector(6 downto 0);	--
			hex2	: out	std_logic_vector(6 downto 0);	--
			hex3	: out	std_logic_vector(6 downto 0);	--
			hex4	: out	std_logic_vector(6 downto 0);	--
			hex5	: out	std_logic_vector(6 downto 0)	--
);
end Labo1;

architecture structural of Labo1 is

	component arcLab1 is
		port (
			clk_clk					: in  std_logic := '0';													--clk.clk
			reset_reset_n			: in	std_logic := '0';													--reset.reset_n
			leds_o_export			: out	std_logic_vector(1 downto 0);									--leds_o.export
			sw_i_export				: in	std_logic_vector(9 downto 0) := (others => '0');			--switches_i.export
			disp_0_to_2_o_export	: out	std_logic_vector(23 downto 0);									--leds_o.export
			disp_3_to_5_o_export	: out	std_logic_vector(23 downto 0);									--leds_o.export
			bt_i_export				: in	std_logic_vector(3 downto 0) := (others => '0')			--switches_i.export
		);
	end component;
	

begin
	nios_system : arcLab1
	port map(	clk_clk					=> clock_50,
					reset_reset_n			=> key(3),
					leds_o_export			=> ledr,
					sw_i_export				=> sw,
					disp_0_to_2_o_export(6 downto 0) 	=> hex0,
					disp_0_to_2_o_export(14 downto 8) 	=> hex1,
					disp_0_to_2_o_export(22 downto 16) 	=> hex2,
					disp_3_to_5_o_export(6 downto 0) 	=> hex3,
					disp_3_to_5_o_export(14 downto 8) 	=> hex4,
					disp_3_to_5_o_export(22 downto 16) 	=> hex5,
					bt_i_export	(2 downto 0)			=> key(2 downto 0)
	);

	
end structural;